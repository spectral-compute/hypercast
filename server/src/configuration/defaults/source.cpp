#include "configuration/defaults.hpp"

#include "configuration/configuration.hpp"

#include "ffmpeg/ffprobe.hpp"
#include "media/AudioCodecInfo.hpp"
#include "util/asio.hpp"

#include <numeric>
#include <stdexcept>

/// @addtogroup configuration_implementation
/// @{

namespace
{

/**
 * Scale a known value by a ratio formed by a counterpart.
 *
 * This is intended to scale the horizontal and vertical resolution in proportion.
 *
 * @param known The known quantity.
 * @param knownCounterpart The counterpart to the known quantity in the ratio.
 * @param unknownCounterpart The counterpart to the unknown quantity in the ratio.
 * @return The unknown quantity. This will be such that, subject to rounding error, the ratio known:return will be the
 *         same as knownCounterpart:unknownCounterpart.
 */
unsigned int getInProportion(unsigned int known, unsigned int knownCounterpart, unsigned int unknownCounterpart)
{
    return (known * unknownCounterpart + known / 2) / knownCounterpart;
}

/**
 * Erase elements from a container if the result would not be an empty container.
 *
 * @param container The container to erase elements from.
 * @param fn A predicate. An element is only erased if this predicate is true for that element.
 */
template <typename T, typename F>
void eraseIfNotEmpty(T &container, F &&fn)
{
    T tmp = container;
    std::erase_if(tmp, std::forward<F>(fn));
    if (!tmp.empty()) {
        container = std::move(tmp);
    }
}

void calculateVideoResolution(Config::VideoQuality &q, const MediaInfo::VideoStreamInfo &info)
{
    /* No resolution at all. */
    if (!q.width && !q.height) {
        q.width = info.width;
        q.height = info.height;
    }

    /* Calculate the height to be proportional to the given width. */
    else if (q.width && !q.height) {
        q.height = getInProportion(*q.width, info.width, info.height);
    }

    /* Calculate the width to be proportional to the given height. */
    else if (!q.width && q.height) {
        q.width = getInProportion(*q.height, info.height, info.width);
    }
}

void calculateVideoFrameRate(Config::FrameRate &frameRate, const MediaInfo::VideoStreamInfo &info,
                             unsigned int minFps = 0)
{
    /* Figure out if the fraction reduces the frame rate. */
    bool reducesFps = frameRate.numerator < frameRate.denominator;

    /* Multiply the fraction by the real frame rate. */
    frameRate.numerator *= info.frameRateNumerator;
    frameRate.denominator *= info.frameRateDenominator;

    /* Make sure we don't reduce the frame rate below the minimum if we're not allowed to. */
    // This integer division may round down, but only if the unrounded result would be less than the next integer
    // anyway, so the less than operator works.
    if (reducesFps && frameRate.numerator / frameRate.denominator < minFps) {
        frameRate.numerator = info.frameRateNumerator;
        frameRate.denominator = info.frameRateDenominator;
    }

    /* Simplify the fraction. Probably not technically necessary, but it's nice :) */
    unsigned int gcd = std::gcd(frameRate.numerator, frameRate.denominator);
    frameRate.numerator /= gcd;
    frameRate.denominator /= gcd;

    /* The frame rate is now in FPS. */
    frameRate.type = Config::FrameRate::fps;
}

unsigned int calculateAudioSampleRate(const MediaInfo::AudioStreamInfo &streamInfo, Codec::AudioCodec codec)
{
    /* Condition 1: compatible sample rates. */
    const Codec::AudioCodecInfo &codecInfo = Codec::AudioCodecInfo::get(codec);
    std::vector<unsigned int> sampleRates(codecInfo.sampleRates.begin(), codecInfo.sampleRates.end());

    /* Condition 2: sample rate <= 48 kHz. */
    eraseIfNotEmpty(sampleRates, [](unsigned int sr) { return sr > 48000; });

    /* Condition 3: sample rate <= input sample rate. */
    eraseIfNotEmpty(sampleRates, [&](unsigned int sr) { return sr > streamInfo.sampleRate; });

    /* Condition 4: GCD >= 32 kHz. This, e.g: chooses 48000 from an original of 96000. */
    eraseIfNotEmpty(sampleRates, [&](unsigned int sr) { return sr < 32000 || streamInfo.sampleRate % sr != 0; });

    /* Condition 5: Choose the highest sample rate of those remaining. */
    assert(!sampleRates.empty());
    return sampleRates.back();
}

} // namespace

/// @}

namespace Config
{

/**
 * Use ffprobe to fill in properties of the media stream, such as resolution, frame rate, and so on.
 *
 * @param qualities The list of qualities to fill in. This is a list so that ffprobe can be run lazily, with the state
 *                  for doing that in this function.
 * @param source The media source configuration.
 */
Awaitable<void> fillInQualitiesFromFfprobe(std::vector<Quality> &qualities, const Source &source,
                                           const ProbeFunction &probe)
{
    /* Lazily initialized information about the media source. The macro is to avoid an expensive co_await for every
       use. */
    #define ensureMediaInfoInitialized() \
        do { \
            if (!mediaInfo) { \
                mediaInfo = co_await probe(source.url, source.arguments); \
                if (!mediaInfo->video) { \
                    throw std::runtime_error("Media source has no video."); \
                } \
            } \
        } while (false)
    std::optional<MediaInfo::SourceInfo> mediaInfo;

    /* Fill in any properties that come from the video stream. */
    for (Config::Quality &q: qualities) {
        // Fill in the resolution.
        if (!q.video.width || !q.video.height) {
            ensureMediaInfoInitialized();
            calculateVideoResolution(q.video, *mediaInfo->video);
        }

        // Fill in the frame rate if it's only expressed as a fraction.
        switch (q.video.frameRate.type)
        {
            case Config::FrameRate::fps:
                break;
            case Config::FrameRate::fraction: {
                ensureMediaInfoInitialized();
                calculateVideoFrameRate(q.video.frameRate, *mediaInfo->video);
                break;
            }
            case Config::FrameRate::fraction23: {
                ensureMediaInfoInitialized();
                calculateVideoFrameRate(q.video.frameRate, *mediaInfo->video, 23);
                break;
            }
        }

        // Fill in the audio sample rate.
        if (q.audio.codec != Codec::AudioCodec::none && !q.audio.sampleRate) {
            ensureMediaInfoInitialized();
            if (!mediaInfo->audio) {
                throw std::runtime_error("Media source has no audio, but quality audio codec is not \"none\"."); \
            }
            q.audio.sampleRate = calculateAudioSampleRate(*mediaInfo->audio, q.audio.codec);
        }
    }

    /* Done :) */
    #undef ensureMediaInfoInitialized
}

} // namespace Config
