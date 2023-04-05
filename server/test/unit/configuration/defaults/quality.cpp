#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"

#include "coro_test.hpp"
#include "data.hpp"

namespace
{

/**
 * Represents a value that we're testing the default-setting behaviour of.
 *
 * There are two possible cases for this:
 * 1. A value that's going to be set by default and should be in some range.
 * 2. A value that we're specifying explicitly.
 */
class TestValue final
{
public:
    /**
     * Construct a test value with a set initial value to be put into the configuration.
     *
     * @param initialValue The initial value to give the configuration, and to expect back.
     */
    TestValue(unsigned int initialValue) : initialValue(initialValue), minimum(initialValue), maximum(initialValue) {}

    /**
     * Construct a value that the default setter has to fill in.
     *
     * @param minimum The minimum value the default setter should choose.
     * @param maximum The maximum value the default setter should choose.
     */
    TestValue(unsigned int minimum, unsigned int maximum) : minimum(minimum), maximum(maximum) {}

    /**
     * Get the initial value.
     */
    const std::optional<unsigned int> &operator*() const
    {
        return initialValue;
    }

    /**
     * Check that a given value is in range.
     */
    void operator()(const char *name, unsigned int value) const
    {
        if (initialValue) {
            EXPECT_EQ(value, *initialValue) << "Set value of " << *initialValue << " for " << name << "was changed to "
                                            << value << ".";
        }
        else {
            EXPECT_TRUE(value >= minimum && value <= maximum) << "Chosen default of " << value << " for " << name
                                                              << " is out of range [" << minimum << ", " << maximum << "]";
        }
    }

private:
    std::optional<unsigned int> initialValue;
    unsigned int minimum;
    unsigned int maximum;
};

Awaitable<void> test(IOContext &ioc,

                     // Video parameters.
                     TestValue bitrate, TestValue minBitrate, TestValue rateControlBufferLength, TestValue gop,

                     // Interleave parameters.
                     TestValue minInterleaveRate, TestValue minInterleaveWindow,

                     // Client buffer parameters.
                     TestValue extraBuffer, TestValue initialBuffer, TestValue seekBuffer, TestValue minimumInitTime,

                     // Input.
                     unsigned int targetLatency = 2000, Config::Root config = {},
                     unsigned int width = 1920, unsigned int height = 1080,
                     unsigned int frameRateNumerator = 25, unsigned int frameRateDemoninator = 1, bool hasAudio = true)
{
    /* Make sure /live exists. */
    if (!config.channels.count("/live")) {
        config.channels["/live"] = {};
    }

    /* Fill in stuff that comes from the source. */
    config.channels.at("/live").source.url = "file";

    config.channels.at("/live").qualities.resize(1);
    Config::Quality &q = config.channels.at("/live").qualities[0];

    q.targetLatency = targetLatency;
    q.video.width = width;
    q.video.height = height;
    q.video.frameRate.type = Config::FrameRate::fps;
    q.video.frameRate.numerator = frameRateNumerator;
    q.video.frameRate.denominator = frameRateDemoninator;

    if (hasAudio) {
        q.audio.sampleRate = 48000;
    }

    /* Make a list of configuration fields and their corresponding test values (and their names). */
    std::initializer_list<std::tuple<const char *, std::optional<unsigned int> &, const TestValue &>> fields = {
        { "q.video.bitrate", q.video.bitrate, bitrate },
        { "q.video.minBitrate", q.video.minBitrate, minBitrate },
        { "q.video.rateControlBufferLength", q.video.rateControlBufferLength, rateControlBufferLength },
        { "q.video.gop", q.video.gop, gop },

        { "q.minInterleaveRate", q.minInterleaveRate, minInterleaveRate },
        { "q.minInterleaveWindow", q.minInterleaveWindow, minInterleaveWindow },

        { "q.clientBufferControl.extraBuffer", q.clientBufferControl.extraBuffer, extraBuffer },
        { "q.clientBufferControl.initialBuffer", q.clientBufferControl.initialBuffer, initialBuffer },
        { "q.clientBufferControl.seekBuffer", q.clientBufferControl.seekBuffer, seekBuffer },
        { "q.clientBufferControl.minimumInitTime", q.clientBufferControl.minimumInitTime, minimumInitTime }
    };

    /* Fill in the optional parts of the configuration. */
    for (auto &[_, configValue, testValue]: fields) {
        configValue = *testValue;
    }

    /* Fill in the defaults. */
    co_await fillInDefaults(ioc, config);

    // Check that this didn't create a new quality.
    EXPECT_EQ(1, config.channels.at("/live").qualities.size());
    if (config.channels.at("/live").qualities.empty()) {
        co_return;
    }
    EXPECT_EQ(&q, &config.channels.at("/live").qualities[0]);
    if (&config.channels.at("/live").qualities[0] != &q) {
        co_return;
    }

    /* Check that the quality got filled in sanely. */
    for (auto &[name, configValue, testValue]: fields) {
        EXPECT_TRUE(configValue);
        if (!configValue) {
            co_return;
        }
        testValue(name, *configValue);
    }

    /* Check that bitrates are sensible relative to each other. */
    EXPECT_LT(*q.video.minBitrate, *q.video.bitrate);
    EXPECT_LT(*q.minInterleaveRate, *q.video.minBitrate + (hasAudio ? q.audio.bitrate : 0));

    /* Produce our own estimate of the latency, and check that it does not exceed the target latency by more than a
       tiny amount. */
    // Remember that the jitter buffer the client keeps is included in the interleave minimum latency.
    double explicitLatency = (config.network.transitLatency + config.network.transitJitter) / 1000.0;
    double sourceLatency = *config.channels.at("/live").source.latency / 1000.0;

    double interleaveRateLatency = config.network.transitBufferSize / (*q.minInterleaveRate * 125.0);
    double interleaveWindowLatency = *q.minInterleaveWindow / 1000.0;

    double clientLatency = std::max(*q.clientBufferControl.extraBuffer, *q.clientBufferControl.seekBuffer) / 1000.0;

    double latency = explicitLatency + sourceLatency + interleaveRateLatency + interleaveWindowLatency + clientLatency;
    EXPECT_LE(latency * 1000, targetLatency + 10) <<
        "Explicit latency: " << (explicitLatency * 1000.0) << " ms\n" <<
        "Source latency: " << (sourceLatency * 1000.0) << " ms\n" <<
        "Interleave rate latency: " << (interleaveRateLatency * 1000.0) << " ms\n" <<
        "Interleave window latency: " << (interleaveWindowLatency * 1000.0) << " ms\n" <<
        "Client buffer: " << (clientLatency * 1000.0) << " ms\n";

    if (latency * 1000 > targetLatency + 10) {
        printf("Configuration values:\n");
        printf("    q.audio.bitrate: %u\n", q.audio.bitrate);
        for (auto &[name, configValue, testValue]: fields) {
            printf("    %s: %u\n", name, *configValue);
        }
    }
}

// TODO: Improve these test parameters once we've experimented.

CORO_TEST(ConfigQualityDefaults, Default, ioc)
{
    co_await test(
        ioc,

        // Video parameters.
        {2500, 3500}, {250, 500}, {500, 1000}, {375, 375},

        // Interleave parameters.
        {150, 250}, {100, 250},

        // Client buffer parameters.
        {100, 700}, {500, 2000}, {50, 350}, {500, 2000}

        // Input.
    );
}

CORO_TEST(ConfigQualityDefaults, Default1s, ioc)
{
    co_await test(
        ioc,

        // Video parameters.
        {2500, 3500}, {500, 1000}, {250, 500}, {375, 375},

        // Interleave parameters.
        {300, 700}, {100, 250},

        // Client buffer parameters.
        {100, 700}, {400, 1000}, {50, 350}, {500, 2000},

        // Input.
        1000
    );
}

CORO_TEST(ConfigQualityDefaults, PresetRateControlBufferLength, ioc)
{
    co_await test(
        ioc,

        // Video parameters.
        {2500, 3500}, {250, 500}, 1000, {375, 375},

        // Interleave parameters.
        {150, 250}, {100, 250},

        // Client buffer parameters.
        {100, 700}, {500, 2000}, {50, 350}, {500, 2000}

        // Input.
    );
}

} // namespace
