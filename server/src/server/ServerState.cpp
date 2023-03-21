#include "ServerState.h"
#include "log/MemoryLog.hpp"
#include "log/FileLog.hpp"
#include "resources/FilesystemResource.hpp"
#include "dash/DashResources.hpp"

namespace {


/**
 * Create a log based on the configuration specification of it.
 */
std::unique_ptr<Log::Log> createLog(const Config::Log &config, IOContext &ioc)
{
    if (config.path.empty()) {
        return std::make_unique<Log::MemoryLog>(ioc, config.level, *config.print);
    }
    return std::make_unique<Log::FileLog>(ioc, config.path, config.level, *config.print);
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


/// Perform initial setup/configuration.
Server::State::State(
    const Config::Root& initialCfg,
    IOContext& ioc
):
    ioc(ioc),
    config(),
    log(createLog(initialCfg.log, ioc)),
    server(ioc, *log, initialCfg.network.port, initialCfg.http)
{
    applyConfiguration(initialCfg);
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
void Server::State::applyConfiguration(const Config::Root& newCfg) {
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
    if (config.paths.directories != newCfg.paths.directories) {
        // Delete all the static file resources, and then add all the new ones.
        for (const auto &[path, directory]: config.paths.directories) {
            server.removeResource(path);
        }
        addFilesystemPathsToServer(server, newCfg.paths.directories, ioc);
    }

    // dash.expose is currently only read during the constructor.
    CANT_CHANGE(dash.expose);

    // TODO: Something not-wrong here (I don't understand this object properly).
    Dash::DashResources dash(ioc, *log, config, server);

    // TODO: Start/stop streaming here as needed etc.

    config = newCfg;
    performingStartup = false;

#undef CANT_CHANGE
}
