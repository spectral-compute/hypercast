#include "api/ConfigResource.h"
#include "api/FullConfigResource.hpp"
#include "api/ProbeResource.hpp"
#include "configuration/configuration.hpp"
#include "configuration/defaults.hpp"
#include "instance/ChannelsIndexResource.hpp"
#include "instance/State.hpp"
#include "log/FileLog.hpp"
#include "server/HttpServer.hpp"
#include "server/Path.hpp"
#include "util/Event.hpp"
#include "util/util.hpp"

#include <stdexcept>

using namespace std::string_literals;

namespace
{

/**
 * Read, parse, and validate the configuration from a file.
 */
Config::Root loadConfig(const std::filesystem::path &path)
{
    std::vector<std::byte> bytes = Util::readFile(path);
    return Config::Root::fromJson(std::string_view((const char *)bytes.data(), bytes.size()));
}

} // namespace

int main(int argc, const char * const *argv)
{
    try {
        /* Parse the argument. */
        if (argc != 2) {
            throw std::runtime_error("Usage: "s + (argc ? argv[0] : "") + " configuration.json");
        }
        std::filesystem::path configPath = argv[1];

        /* Load and populate a config object. */
        Config::Root config = loadConfig(configPath);

        /* Create the instance state. */
        IOContext ioc;
        Instance::State st{config, ioc};

        /* Create global resources for the API. */
        st.getServer().addResource<Api::ConfigResource>("api/config", st, configPath);
#ifndef NDEBUG
        st.getServer().addResource<Api::FullConfigResource>("api/full_config", st.getConfiguration());
#endif // NDEBUG
        st.getServer().addResource<Api::ProbeResource>("api/probe", ioc, st.getInUseUrls());

        /* Create other instance global resources. */
        if (config.features.channelIndex) {
            st.getServer().addResource<Instance::ChannelsIndexResource>("channelIndex.json",
                                                                        st.getConfiguration().channels);
        }

        /* Do the asynchronous stuff. */
        spawnDetached(ioc, [&]() -> Awaitable<void> {
            try {
                // Run the configuration application process.
                co_await st.applyConfiguration(config);

                // Hang this coroutine forever. Interesting things happen as a result of the server handling requests
                // in another coroutine.
                Event event(ioc);
                while (true) {
                    co_await event.wait();
                }
            }
            catch (const std::exception &e) {
                fprintf(stderr, "Exited with exception: %s\n", e.what());
            }
            catch (...) {
                fprintf(stderr, "Exited with an unknown exception.\n");
            }
        });
        ioc.run();
    }
    catch (const std::exception &e) {
        fprintf(stderr, "Exited with exception: %s\n", e.what());
    }
    catch (...) {
        fprintf(stderr, "Exited with an unknown exception.\n");
    }

    /* The program should run indefinitely unless there's an error. */
    return 1;
}
