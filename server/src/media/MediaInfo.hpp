#pragma once

#include <optional>

/**
 * @defgroup media Media
 *
 * Stuff for representing information about media sources.
 */
/// @addtogroup media
/// @{

/**
 * Information about a media source.
 *
 * If multiple audio or video streams are present in the source, then only the one that would be used
 */
namespace MediaInfo
{

/**
 * Information about a video stream.
 */
struct VideoStreamInfo final
{
    /**
     * The width of the video in (square) pixels.
     */
    unsigned int width = 0;

    /**
     * The height of the video in (square) pixels.
     */
    unsigned int height = 0;

    /**
     * The numerator of the frame rate in frames per second.
     */
    unsigned int frameRateNumerator = 0;

    /**
     * The denominator of the frame rate in frames per second.
     */
    unsigned int frameRateDenominator = 0;

#ifdef WITH_TESTING
    bool operator==(const VideoStreamInfo &) const = default;
#endif // WITH_TESTING
};

/**
 * Information about an audio stream.
 */
struct AudioStreamInfo final
{
    /**
     * The sample rate of the audio in samples per second.
     */
    unsigned sampleRate = 0;

#ifdef WITH_TESTING
    bool operator==(const AudioStreamInfo &) const = default;
#endif // WITH_TESTING
};

/**
 * Information about a media source.
 */
struct SourceInfo final
{
    /**
     * The default video stream.
     */
    std::optional<VideoStreamInfo> video;

    /**
     * The default audio stream.
     */
    std::optional<AudioStreamInfo> audio;

#ifdef WITH_TESTING
    bool operator==(const SourceInfo &) const = default;
#endif // WITH_TESTING
};

} // namespace MediaInfo

/// @}
