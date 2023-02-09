#include "configuration/configuration.hpp"
#include "configuration/defaults.hpp"
#include "dash/DashResources.hpp"
#include "log/FileLog.hpp"
#include "log/MemoryLog.hpp"
#include "resources/FilesystemResource.hpp"
#include "server/HttpServer.hpp"
#include "server/Path.hpp"
#include "util/Event.hpp"
#include "util/util.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <memory>
#include <stdexcept>

using namespace std::string_literals;

namespace
{

/**
 * Read, parse, and validate the configuration from a file.
 */
Config::Root loadConfig(const char *path)
{
    std::vector<std::byte> bytes = Util::readFile(path);
    return Config::Root::fromJson(std::string_view((const char *)bytes.data(), bytes.size()));
}

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

/**
 * Asynchronous version of main.
 */
Awaitable<void> asyncMain(int argc, const char * const *argv, IOContext &ioc)
{
    /* Read the configuration. */
    if (argc != 2) {
        throw std::runtime_error("Usage: "s + (argc ? argv[0] : "") + " configuration.json");
    }
    Config::Root config = loadConfig(argv[1]);
    co_await Config::fillInDefaults(ioc, config);

    /* Create a log file. */
    std::unique_ptr<Log::Log> log = createLog(config.log, ioc);

    /* Create the server. */
    Server::HttpServer server(ioc, *log, config.network.port, config.http);

    /* Add stuff to the server. */
    addFilesystemPathsToServer(server, config.paths.directories, ioc);
    Dash::DashResources dash(ioc, *log, config, server);

    /* Start FFmpeg streaming DASH into the server. */

    /* Keep the above objects in scope indefinitely. */
    Event event(ioc);
    while (true) {
        co_await event.wait();
    }
}

} // namespace

int main(int argc, const char * const *argv)
{
    /* This is just a wrapper around boost::asio and asyncMain. */
    IOContext ioc;
    boost::asio::co_spawn((boost::asio::io_context &)ioc, [&]() -> boost::asio::awaitable<void> {
        try {
            co_await asyncMain(argc, argv, ioc);
        }
        catch (const std::exception &e) {
            fprintf(stderr, "Exited with exception: %s\n", e.what());
        }
        catch (...) {
            fprintf(stderr, "Exited with an unknown exception.\n");
        }
    }, boost::asio::detached);
    ioc.run();
    return 1; // The program should run indefinitely unless there's an error.
}
