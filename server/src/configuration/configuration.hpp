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

    bool operator==(const Source &) const;
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

    bool operator==(const FrameRate &) const;
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

    bool operator==(const VideoQuality &) const;
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

    bool operator==(const AudioQuality &) const;
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

    bool operator==(const ClientBufferControl &) const;
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

    bool operator==(const Quality &) const;
};

/**
 * The dash key.
 */
struct Dash final
{
    unsigned int segmentDuration = 15000;
    bool expose = false;
    unsigned int preAvailabilityTime = 4000;

    bool operator==(const Dash &) const;
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

    bool operator==(const Network &) const;
};

/**
 * The http key.
 */
struct Http final
{
    std::optional<std::string> origin = "*";
    unsigned int cacheNonLiveTime = 600;

    bool operator==(const Http &) const;
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

    bool operator==(const Directory &) const;
};

/**
 * The paths key.
 */
struct Paths final
{
    std::string liveInfo = "/live/info.json";
    std::string liveStream = "/live/{uid}";
    std::map<std::string, Directory> directories;

    bool operator==(const Paths &) const;
};

/**
 * The history key.
 */
struct History final
{
    unsigned int historyLength = 90;

    bool operator==(const History &) const;
};

/**
 * The log key.
 */
struct Log final
{
    std::string path;
    std::optional<bool> print;
    ::Log::Level level = ::Log::Level::info;

    bool operator==(const Log &) const;
};

/**
 * The root of the configuration.
 */
class Root final
{
public:
#ifndef WITH_TESTING
    // These methods are incompatible with designated initialization, which is useful for testing. But their presence
    // here when not testing avoids generating complicated code automatically in lots of places.
    ~Root();

    Root(const Root &);
    Root(Root &&) noexcept;
    Root &operator=(const Root &);
    Root &operator=(Root &&) noexcept;
#endif // WITH_TESTING

    /**
     * Load configuration from a JSON formatted string.
     *
     * @param jsonString The configuration object as a JSON formatted string.
     */
    static Root fromJson(std::string_view jsonString);

    // The json this object was originally decoded from. If it was mutated afterwards, this will not be in sync.
    std::string jsonRepresentation;

    Source source;
    std::vector<Quality> qualities;
    Dash dash;
    Network network;
    Http http;
    Paths paths;
    History history;
    Log log;

    bool operator==(const Root &) const;

private:
#ifndef WITH_TESTING
    // Making this private enforces the use of fromJson for construction, but is also incompatible with the tests' use
    // of designated initializers.

    /**
     * Perform initial construction with a lot of defaults already filled in.
     *
     * This is used by fromJson().
     */
    Root() = default;
#endif // WITH_TESTING

    /**
     * Validate a loaded configuration.
     */
    void validate() const;
};

} // namespace Config

/// @}
