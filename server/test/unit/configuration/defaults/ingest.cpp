#include "TestImpl.hpp"

#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"

#include "coro_test.hpp"

namespace
{

CORO_TEST(ConfigDefaults, IngestListen, ioc)
{
    /* Fill in initial defaults for a simple configuration. */
    Config::Root config = {
        .channels = {
            {
                "/live", {
                    .source = {
                        .url = "rtmp://localhost:1935/test",
                        .listen = true
                    },

                    // Specify enough of the quality to stop ffprobe from running.
                    .qualities = {
                        {
                            .video = {
                                .width = 1920,
                                .height = 1080,
                                .frameRate = {
                                    .type = Config::FrameRate::fps,
                                    .numerator = 25,
                                    .denominator = 1
                                }
                            },
                            .audio = {
                                .sampleRate = 48000
                            }
                        }
                    }
                }
            }
        }
    };
    fillInInitialDefaults(config);

    /* Check the result. */
    {
        // Check that the channel source got updated.
        const Config::Source &src = config.channels.at("/live").source;
        EXPECT_EQ(src.url, "ingest://__listen__/0");
        EXPECT_FALSE(src.listen);

        // Check that the separated ingest got created.
        const Config::SeparatedIngestSource &ingest = config.separatedIngestSources.at("__listen__/0");
        EXPECT_EQ(ingest.url, "rtmp://localhost:1935/test");
        EXPECT_EQ(ingest.arguments, std::vector<std::string>({"-listen", "1"}));
    }

    /* Fill in the rest of the defaults for the configuration. */
    co_await fillInDefaults(ioc, config);

    /* Check that the configuration got updated correctly. */
    {
        // Check that the channel source got updated.
        const Config::Source &src = config.channels.at("/live").source;
        EXPECT_EQ(src.url, "ingest_http://localhost:8080/ingest/__listen__/0");
        EXPECT_FALSE(src.listen);

        // Check that the separated ingest still exists.
        const Config::SeparatedIngestSource &ingest = config.separatedIngestSources.at("__listen__/0");
        EXPECT_EQ(ingest.url, "rtmp://localhost:1935/test");
        EXPECT_EQ(ingest.arguments, std::vector<std::string>({"-listen", "1"}));
    }
}

} // namespace
