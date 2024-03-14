#include "TestImpl.hpp"

#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"

#include "coro_test.hpp"
#include "data.hpp"

namespace
{

Awaitable<void> test(IOContext &ioc, unsigned int sourceNumerator, unsigned int sourceDenominator,
                     decltype(Config::FrameRate::type) initialType,
                     unsigned int initialNumerator, unsigned int initialDenominator,
                     unsigned int finalNumerator, unsigned int finalDenominator)
{
    /* Fill in defaults for a simple configuration. */
    Config::Root config = {
        .channels = {
            {
                "/live", {
                    .source = {
                        .url = getSmpteDataPath(1920, 1080, sourceNumerator, sourceDenominator, 48000).string()
                    },
                    .qualities = {
                        {
                            .video = {
                                .frameRate = {
                                    .type = initialType,
                                    .numerator = initialNumerator,
                                    .denominator = initialDenominator
                                }
                            }
                        }
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
    EXPECT_EQ((Config::FrameRate{
        .type = Config::FrameRate::fps,
        .numerator = finalNumerator,
        .denominator = finalDenominator
    }), q.video.frameRate);
}

CORO_TEST(ConfigDefaults, FpsHalfPlusNoHalve, ioc)
{
    co_await test(ioc, 25, 1, Config::FrameRate::fraction23, 1, 2, 25, 1);
}

CORO_TEST(ConfigDefaults, FpsHalfPlusHalve, ioc)
{
    co_await test(ioc, 50, 1, Config::FrameRate::fraction23, 1, 2, 25, 1);
}

CORO_TEST(ConfigDefaults, FpsFixed, ioc)
{
    co_await test(ioc, 25, 1, Config::FrameRate::fps, 30, 1, 30, 1);
}

} // namespace

