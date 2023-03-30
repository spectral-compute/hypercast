#include "ServerState.h"
#include "ffmpeg/ffmpeg.hpp"
#include "log/MemoryLog.hpp"
#include "log/FileLog.hpp"
#include "resources/FilesystemResource.hpp"
#include "dash/DashResources.hpp"
#include "configuration/defaults.hpp"
#include "api/ConfigResource.h"

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
                                                       !directory.secure);
    }
}


} // Anonymous namespace

/**
 * Represents state for a single channel.
 */
struct Server::State::Channel final
{
    /**
     * Start streaming.
     */
    explicit Channel(IOContext &ioc, Log::Log &log, const Config::Root &config, const Config::Channel &channelConfig,
                     const std::string &basePath, Server &server) :
        dash(ioc, log, channelConfig, config.http, basePath, server),
        ffmpeg(ioc, log, Ffmpeg::getFfmpegArguments(channelConfig, config.network, (std::string)dash.getUidPath()))
    {
    }

    /**
     * The set of resources that the ffmpeg process streams to (and that converts this from DASH to RISE).
     */
    Dash::DashResources dash;

    /**
     * The ffmpeg subprocess that's streaming to the server.
     */
    Ffmpeg::FfmpegProcess ffmpeg;
};

Server::State::~State() = default;

/// Perform initial setup/configuration.
Server::State::State(
    const Config::Root& initialCfg,
    IOContext& ioc
):
    ioc(ioc),
    requestedConfig(initialCfg),
    log(createLog(initialCfg.log, ioc)),
    server(ioc, *log, initialCfg.network, initialCfg.http)
{
    server.addResource<Api::ConfigResource>("api/config", *this);
}

/// Used to throw exceptions if you try to change a setting that isn't allowed to change except on startup.
void Server::State::configCannotChange(bool itChanged, const std::string& name) const {
    if (!performingStartup && itChanged) {
        throw std::runtime_error("This configuration field cannot be changed at runtime: " + name);
    }
}

/// Change the settings. Add as much clever incremental reconfiguration logic here as you like.
/// Various options are re-read every time they're used and don't require explicit reconfiguration,
/// so they don't appear specifically within this function.
Awaitable<void> Server::State::applyConfiguration(Config::Root newCfg) {
    // TODO: We probably need a mutex to stop this method from running in parallel with itself in another coroutine.
    requestedConfig = newCfg;

    // Fill in the blanks...
    co_await Config::fillInDefaults(ioc, newCfg);

#define CANT_CHANGE(N) configCannotChange(config.N != newCfg.N, #N)
    // Listen port can be changed only by restarting the process (and will probably break
    // the settings UI if you're doing that on one of the hardware units).
    CANT_CHANGE(network.port);

    // Reconfigure the logger.
    if (config.log != newCfg.log) {
        CANT_CHANGE(log.path);
        log->reconfigure(newCfg.log.level, newCfg.log.print.value_or(true));
    }

    // Reconfigure the static file server.
    // TODO: Sort out `liveInfo` and `liveStream`: perhaps by making them not exist. Why aren't they just
    //       hardcoded to be computed based on name of a stream?
    CANT_CHANGE(directories); // TODO: More intelligent determination of which directories to delete.
    if (performingStartup) {
        addFilesystemPathsToServer(server, newCfg.directories, ioc);
    }

    /* Update each channel. */
    for (const auto &[channelPath, channelConfig]: newCfg.channels) {
        // TODO: Stop streaming here as needed.
        if (channels.contains(channelPath)) {
            // TODO: Add a method to Dash::DashResources() to remove its resources, and to Ffmpeg::FfmpegProcess() to
            //       terminate the process. Until then, development of the UI may require commenting out the creation of
            //       channel.
            throw std::runtime_error("Restarting the stream (for " + channelPath + ") is not yet supported.");
        }
    }

    /* Move the configuration to its final location so  */
    // TODO: Either this needs to update only the channels we stopped above, or the channels need a copy of the
    //       configuration. Exactly what we do here depends on how we hand over a channel from one ffmpeg/DashResources
    //       to the next. If we do it by having both in parallel, then it'll need to be a copy.
    config = std::move(newCfg);

    /* Start streaming. */
    for (const auto &[channelPath, channelConfig]: config.channels) {
        if (channels.contains(channelPath)) {
            continue;
        }
        channels.emplace(std::piecewise_construct, std::forward_as_tuple(channelPath),
                         std::forward_as_tuple(ioc, *log, config, channelConfig, channelPath, server));
    }

    /* Mark that we're done performing setup. */
    performingStartup = false;

#undef CANT_CHANGE
}
