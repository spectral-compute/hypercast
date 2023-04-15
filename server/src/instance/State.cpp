#include "State.hpp"
#include "ffmpeg/Arguments.hpp"
#include "ffmpeg/ffprobe.hpp"
#include "ffmpeg/Process.hpp"
#include "log/MemoryLog.hpp"
#include "log/FileLog.hpp"
#include "media/MediaInfo.hpp"
#include "resources/FilesystemResource.hpp"
#include "dash/DashResources.hpp"
#include "configuration/defaults.hpp"

namespace {


/**
 * Create a log based on the configuration specification of it.
 */
std::unique_ptr<Log::Log> createLog(const Config::Log &config, IOContext &ioc)
{
    if (config.path.empty()) {
        return std::make_unique<Log::MemoryLog>(ioc, config.level, config.print ? *config.print : true);
    }
    return std::make_unique<Log::FileLog>(ioc, config.path, config.level, config.print ? *config.print : false);
}

/**
 * Add directories that get served verbatim to the server.
 */
void addFilesystemPathsToServer(Server::Server &server, const std::map<std::string, Config::Directory> &directories,
                                IOContext &ioc)
{
    for (const auto &[path, directory]: directories) {
        server.addResource<Server::FilesystemResource>(path, ioc, directory.localPath, directory.index,
                                                       directory.ephemeral ? Server::CacheKind::ephemeral :
                                                                             Server::CacheKind::fixed,
                                                       !directory.secure, directory.maxWritableSize << 20);
    }
}

} // Anonymous namespace

/**
 * Represents state for a single channel.
 */
struct Instance::State::Channel final
{
    /**
     * Start streaming.
     */
    explicit Channel(IOContext &ioc, Log::Log &log, const Config::Root &config, const Config::Channel &channelConfig,
                     const std::string &basePath, Server::Server &server) :
        dash(ioc, log, channelConfig, config.http, basePath, server),
        ffmpeg(ioc, log, Ffmpeg::Arguments(channelConfig, config.network, (std::string)dash.getUidPath()))
    {
    }

    /**
     * The set of resources that the ffmpeg process streams to (and that converts this from DASH to RISE).
     */
    Dash::DashResources dash;

    /**
     * The ffmpeg subprocess that's streaming to the server.
     */
    Ffmpeg::Process ffmpeg;
};

Instance::State::~State() = default;

/// Perform initial setup/configuration.
Instance::State::State(const Config::Root &initialCfg, IOContext &ioc) :
    ioc(ioc),
    requestedConfig(initialCfg),
    mutex(ioc),
    log(createLog(initialCfg.log, ioc)),
    server(ioc, *log, initialCfg.network, initialCfg.http)
{
}

/// Used to throw exceptions if you try to change a setting that isn't allowed to change except on startup.
void Instance::State::configCannotChange(bool itChanged, const std::string &name) const
{
    if (!performingStartup && itChanged) {
        throw BadConfigurationReplacementException("This configuration field cannot be changed at runtime: " + name);
    }
}

/// Change the settings. Add as much clever incremental reconfiguration logic here as you like.
/// Various options are re-read every time they're used and don't require explicit reconfiguration,
/// so they don't appear specifically within this function.
Awaitable<void> Instance::State::applyConfiguration(Config::Root newCfg)
{
    Mutex::LockGuard lock = co_await mutex.lockGuard();

    // Fill in the blanks...
    std::set<std::string> newInUseUrls;
    std::vector<Ffmpeg::ProbeResult> probes; // Optimization to keep the probe from running twice.
    co_await Config::fillInDefaults([this, &newInUseUrls, &probes](const std::string &url,
                                                                   const std::vector<std::string> &arguments) ->
                                    Awaitable<MediaInfo::SourceInfo> {
        assert(!newInUseUrls.contains(url)); // This should be imposed by Config::Root::validate.
        inUseUrls.emplace(url); // The new and old URLs are in use at the same time now, but just temporarily.
        newInUseUrls.emplace(url);
        probes.emplace_back(co_await Ffmpeg::ffprobe(ioc, url, arguments));
        co_return probes.back();
    }, newCfg);

#define CANT_CHANGE(N) configCannotChange(config.N != newCfg.N, #N)
    // Listen port can be changed only by restarting the process (and will probably break
    // the settings UI if you're doing that on one of the hardware units).
    CANT_CHANGE(network.port);
    CANT_CHANGE(network.publicPort);

    // We don't currently have the code to change these.
    CANT_CHANGE(http.ephemeralWhenNotFound);
    CANT_CHANGE(features);

    // Reconfigure the logger.
    if (config.log != newCfg.log) {
        CANT_CHANGE(log.path);
        log->reconfigure(newCfg.log.level, newCfg.log.print.value_or(true));
    }

    // Reconfigure the static file server.
    CANT_CHANGE(directories); // TODO: More intelligent determination of which directories to delete.
    if (performingStartup) {
        addFilesystemPathsToServer(server, newCfg.directories, ioc);
    }

    // Delete channels that are simply gone.
    std::vector<std::string> murderise;
    for (auto &[channelPath, channelConfig]: channels) {
        if (!newCfg.channels.contains(channelPath)) {
            co_await channelConfig.ffmpeg.kill();
            murderise.push_back(channelPath);
        }
    }
    for (const auto& p : murderise) {
        channels.erase(p);
    }

    // Update channels that have been.. updated.
    for (const auto &[channelPath, channelConfig]: newCfg.channels) {
        if (channels.contains(channelPath)) {
            // Only restart streaming if the channel configuration changed.
            if (channelConfig.differsByUidOnly(config.channels.at(channelPath))) {
                continue;
            }

            // Destroy the channel, and rely on the code below to recreate it.
            auto& node = channels.at(channelPath);
            co_await node.ffmpeg.kill();

            // Delete the channel. TODO: Can all of the above just... happen in destructors so this is the only line needed?
            channels.erase(channelPath);
        }
    }

    /* Move the configuration to its final location so  */
    // TODO: Either this needs to update only the channels we stopped above, or the channels need a copy of the
    //       configuration. Exactly what we do here depends on how we hand over a channel from one ffmpeg/DashResources
    //       to the next. If we do it by having both in parallel, then it'll need to be a copy.
    config = std::move(newCfg);

    /* Start streaming. */
    inUseUrls = std::move(newInUseUrls); // Now we've finished with the old channels, show the new ones as in use.
    for (const auto &[channelPath, channelConfig]: config.channels) {
        if (channels.contains(channelPath)) {
            continue;
        }
        auto it = channels.emplace(std::piecewise_construct, std::forward_as_tuple(channelPath),
                                   std::forward_as_tuple(ioc, *log, config, channelConfig, channelPath, server));
        co_await it.first->second.ffmpeg.waitForProbe(); // Avoid running ffprobe redundantly.
    }

    /* Now that we got here, we successfully applied the new configuration, so record it as the new requested
       configuration. */
    // TODO: I think we actually only care about jsonRepresentation from this, in which case, it probably makes sense to
    //       move that from Config::Root to either this class or Api::ConfigResource.
    requestedConfig = config;

    /* Mark that we're done performing setup. */
    performingStartup = false;

#undef CANT_CHANGE
}
