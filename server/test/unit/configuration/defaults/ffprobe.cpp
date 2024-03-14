#include "TestImpl.hpp"

#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"

#include "coro_test.hpp"
#include "data.hpp"

namespace
{

CORO_TEST(ConfigDefaults, Ffprobe, ioc)
{
    /* Fill in defaults for a simple configuration. */
    Config::Root config = {
        .channels = {
            {
                "/live", {
                    .source = {
                        .url = getSmpteDataPath(1920, 1080, 25, 1, 48000).string()
                    }
                }
            }
        }
    };
    co_await fillInDefaults(ioc, config);

    /* Make sure we have a quality to test. */
    EXPECT_EQ(1, config.channels.at("/live").qualities.size());
    if (config.channels.at("/live").qualities.empty()) {
        co_return;
    }
    const Config::Quality &q = config.channels.at("/live").qualities[0];

    /* Check that the quality got filled in correctly. */
    EXPECT_EQ(1920, q.video.width);
    EXPECT_EQ(1080, q.video.height);
    EXPECT_EQ((Config::FrameRate{
        .type = Config::FrameRate::fps,
        .numerator = 25,
        .denominator = 1
    }), q.video.frameRate);
    EXPECT_EQ(48000, q.audio.sampleRate);
}

} // namespace
