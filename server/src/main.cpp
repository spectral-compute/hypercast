#include "configuration/configuration.hpp"
#include "configuration/defaults.hpp"
#include "log/FileLog.hpp"
#include "log/MemoryLog.hpp"
#include "server/HttpServer.hpp"
#include "server/Path.hpp"
#include "util/Event.hpp"
#include "util/util.hpp"
#include "server/ServerState.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

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
 * Asynchronous version of main.
 */
Awaitable<void> asyncMain(int argc, const char * const *argv, IOContext &ioc)
{
    /* Read the configuration. */
    if (argc != 2) {
        throw std::runtime_error("Usage: "s + (argc ? argv[0] : "") + " configuration.json");
    }

    // Load and populate a config object.
    Config::Root config = loadConfig(argv[1]);

    Server::State st{config, ioc};
    co_await st.applyConfiguration(config);

    // Hang this coroutine forever. Interesting things happen as a result of the server handling requests
    // in another coroutine.
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
