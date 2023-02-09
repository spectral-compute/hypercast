#include "ffmpeg/ffprobe.hpp"

#include "configuration/configuration.hpp"

#include "coro_test.hpp"
#include "data.hpp"

namespace
{

CORO_TEST(Ffprobe, IntegerFPS, ioc)
{
    Config::Source source = {.url = getSmpteDataPath(1920, 1080, 25, 1, 48000).string()};
    MediaInfo::SourceInfo ffprobed = co_await Ffmpeg::ffprobe(ioc, source);

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
    Config::Source source = {.url = getSmpteDataPath(1920, 1080, 30000, 1001, 48000).string()};
    MediaInfo::SourceInfo ffprobed = co_await Ffmpeg::ffprobe(ioc, source);

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

} // namespace
