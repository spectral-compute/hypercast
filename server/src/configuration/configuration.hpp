#pragma once

#include "log/Level.hpp"
#include "media/codec.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @defgroup configuration Configuration
 *
 * The configuration system.
 */
/// @addtogroup configuration
/// @{

/**
 * Contains streaming server configuration.
 *
 * Most of this is documented in Configuration.md from the server documentation.
 */
namespace Config
{

/**
 * The source key.
 */
struct Source final
{
    std::string url;
    std::vector<std::string> arguments;
    bool loop = false;
    bool timestamp = false;
    std::optional<unsigned int> latency;

#ifdef WITH_TESTING
    bool operator==(const Source &) const;
#endif // WITH_TESTING
};

/**
 * The qualities.video.frameRate key.
 *
 * In JSON, this is a single value, but it's expressed as a composite here.
 */
struct FrameRate final
{
    /**
     * How to interpret or calculate value.
     */
    enum
    {
        /**
         * The value is interpreted as a frame rate in frames per second.
         *
         * This is the form that should be in use after filling in defaults.
         */
        fps,

        /**
         * The value is interpreted as a fraction to multiply the frame rate by.
         */
        fraction,

        /**
         * The value is interpreted as a fraction to multiply the frame rate by if the result is at least 23 fps,
         * otherwise the source frame rate is used.
         */
        fraction23
    } type = fraction;

    /**
     * The numerator of the value of the frame rate.
     */
    unsigned int numerator = 1;

    /**
     * The denominator of the value of the frame rate.
     */
    unsigned int denominator = 1;

#ifdef WITH_TESTING
    bool operator==(const FrameRate &) const;
#endif // WITH_TESTING
};

/**
 * The qualities.video.h26xPreset key.
 */
enum class H26xPreset
{
    ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo
};

/**
 * The qualities.video key.
 */
struct VideoQuality final
{
    std::optional<unsigned int> width;
    std::optional<unsigned int> height;
    FrameRate frameRate;
    std::optional<unsigned int> bitrate;
    std::optional<unsigned int> minBitrate;
    unsigned int crf = 25;
    std::optional<unsigned int> rateControlBufferLength;
    Codec::VideoCodec codec = Codec::VideoCodec::h264;
    H26xPreset h26xPreset = H26xPreset::faster;
    unsigned int vpXSpeed = 8;
    std::optional<unsigned int> gop;

#ifdef WITH_TESTING
    bool operator==(const VideoQuality &) const;
#endif // WITH_TESTING
};

/**
 * The qualities.audio key.
 */
struct AudioQuality final
{
    std::optional<unsigned int> sampleRate;
    unsigned int bitrate = 64;
    Codec::AudioCodec codec = Codec::AudioCodec::aac;

    /**
     * Determine if this audio quality represents actually having audio.
     */
    operator bool() const
    {
        return sampleRate && codec != Codec::AudioCodec::none;
    }

#ifdef WITH_TESTING
    bool operator==(const AudioQuality &) const;
#endif // WITH_TESTING
};

/**
 * The qualities.clientBufferControl key.
 */
struct ClientBufferControl final
{
    std::optional<unsigned int> extraBuffer;
    std::optional<unsigned int> initialBuffer;
    std::optional<unsigned int> seekBuffer;
    std::optional<unsigned int> minimumInitTime;

#ifdef WITH_TESTING
    bool operator==(const ClientBufferControl &) const;
#endif // WITH_TESTING
};

/**
 * The qualities key's elements.
 */
struct Quality final
{
    VideoQuality video;
    AudioQuality audio;
    unsigned int targetLatency = 2000;
    std::optional<unsigned int> minInterleaveRate;
    std::optional<unsigned int> minInterleaveWindow;
    unsigned int interleaveTimestampInterval = 100;
    ClientBufferControl clientBufferControl;

#ifdef WITH_TESTING
    bool operator==(const Quality &) const;
#endif // WITH_TESTING
};

/**
 * The dash key.
 */
struct Dash final
{
    unsigned int segmentDuration = 15000;
    bool expose = false;
    unsigned int preAvailabilityTime = 4000;

#ifdef WITH_TESTING
    bool operator==(const Dash &) const;
#endif // WITH_TESTING
};

/**
 * The network key.
 */
struct Network final
{
    uint16_t port = 8080;
    unsigned int transitLatency = 50;
    unsigned int transitJitter = 200;
    unsigned int transitBufferSize = 32768;

#ifdef WITH_TESTING
    bool operator==(const Network &) const;
#endif // WITH_TESTING
};

/**
 * The http key.
 */
struct Http final
{
    std::optional<std::string> origin = "*";
    unsigned int cacheNonLiveTime = 600;

#ifdef WITH_TESTING
    bool operator==(const Http &) const;
#endif // WITH_TESTING
};

/**
 * A directory to expose.
 */
struct Directory final
{
    std::string localPath;
    std::string index;
    bool secure = false;
    bool ephemeral = false;

#ifdef WITH_TESTING
    bool operator==(const Directory &) const;
#endif // WITH_TESTING
};

/**
 * The paths key.
 */
struct Paths final
{
    std::string liveInfo = "/live/info.json";
    std::string liveStream = "/live/{uid}";
    std::map<std::string, Directory> directories;

#ifdef WITH_TESTING
    bool operator==(const Paths &) const;
#endif // WITH_TESTING
};

/**
 * The history key.
 */
struct History final
{
    unsigned int historyLength = 90;

#ifdef WITH_TESTING
    bool operator==(const History &) const;
#endif // WITH_TESTING
};

/**
 * The log key.
 */
struct Log final
{
    std::string path;
    std::optional<bool> print;
    ::Log::Level level = ::Log::Level::info;

#ifdef WITH_TESTING
    bool operator==(const Log &) const;
#endif // WITH_TESTING
};

/**
 * The root of the configuration.
 */
class Root final
{
public:
#ifndef WITH_TESTING
    ~Root();
#endif // WITH_TESTING

    /**
     * Load configuration from a JSON formatted string.
     *
     * @param jsonString The configuration object as a JSON formatted string.
     */
    static Root fromJson(std::string_view jsonString);

    Source source;
    std::vector<Quality> qualities;
    Dash dash;
    Network network;
    Http http;
    Paths paths;
    History history;
    Log log;

#ifdef WITH_TESTING
    bool operator==(const Root &) const;
#endif // WITH_TESTING

private:
#ifndef WITH_TESTING
    // These methods (and the destructor) are incompatible with designated initialization, which is useful for testing.
    // But their presence here when not testing is useful to enforce that fromJson() has to be used for construction.

    /**
     * Perform initial construction with a lot of defaults already filled in.
     *
     * This is used by fromJson().
     */
    Root() = default;

    /**
     * Used by fromJson().
     */
    Root(Root &&) = default;
#endif // WITH_TESTING

    /**
     * Validate a loaded configuration.
     */
    void validate() const;
};

} // namespace Configuration

/// @}
