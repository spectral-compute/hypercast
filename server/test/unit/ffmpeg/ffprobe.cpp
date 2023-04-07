#include "ffmpeg/ffprobe.hpp"

#include "configuration/configuration.hpp"

#include "coro_test.hpp"
#include "data.hpp"

namespace
{

CORO_TEST(Ffprobe, IntegerFPS, ioc)
{
    MediaInfo::SourceInfo ffprobed = co_await Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 25, 1, 48000).string());

    MediaInfo::SourceInfo ref = {
        .video = MediaInfo::VideoStreamInfo{
            .width = 1920,
            .height = 1080,
            .frameRateNumerator = 25,
            .frameRateDenominator = 1
        },
        .audio = MediaInfo::AudioStreamInfo{
            .sampleRate = 48000
        }
    };

    EXPECT_EQ(ref, ffprobed);
}

CORO_TEST(Ffprobe, FractionFPS, ioc)
{
    MediaInfo::SourceInfo ffprobed =
        co_await Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 30000, 1001, 48000).string());

    MediaInfo::SourceInfo ref = {
        .video = MediaInfo::VideoStreamInfo{
            .width = 1920,
            .height = 1080,
            .frameRateNumerator = 30000,
            .frameRateDenominator = 1001
        },
        .audio = MediaInfo::AudioStreamInfo{
            .sampleRate = 48000
        }
    };

    EXPECT_EQ(ref, ffprobed);
}

CORO_TEST(Ffprobe, DifferentCache, ioc)
{
    Ffmpeg::ProbeResult ffprobed1 = co_await Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 25, 1, 48000).string());
    Ffmpeg::ProbeResult ffprobed2 =
        co_await Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 30000, 1001, 48000).string());
    EXPECT_NE(&*ffprobed1, &*ffprobed2);
}

CORO_TEST(Ffprobe, SameCache, ioc)
{
    // The second ffprobe should return the result from the first because it hasn't expired yet.
    Ffmpeg::ProbeResult ffprobed1 = co_await Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 25, 1, 48000).string());
    Ffmpeg::ProbeResult ffprobed2 = co_await Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 25, 1, 48000).string());
    EXPECT_EQ(&*ffprobed1, &*ffprobed2);
}

CORO_TEST(Ffprobe, ConcurrentCache, ioc)
{
    // The second ffprobe should wait for the first one and then return that result.
    auto [ffprobed1, ffprobed2] = co_await (Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 25, 1, 48000).string()) &&
                                            Ffmpeg::ffprobe(ioc, getSmpteDataPath(1920, 1080, 25, 1, 48000).string()));
    EXPECT_EQ(&*ffprobed1, &*ffprobed2);
}

} // namespace
