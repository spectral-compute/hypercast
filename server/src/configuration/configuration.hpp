#pragma once

#include "log/Level.hpp"
#include "media/codec.hpp"
#include "server/Address.hpp"

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

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
 * The channels.source key.
 */
struct Source final
{
    std::string url;
    std::vector<std::string> arguments;
    bool listen = false;
    bool loop = false;
    bool timestamp = false;
    std::optional<unsigned int> latency;

    bool operator==(const Source &) const;
};

/**
 * The channels.qualities.video.frameRate key.
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
 * The channels.qualities.video.h26xPreset key.
 */
enum class H26xPreset
{
    ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo
};

/**
 * The channels.qualities.video key.
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
    std::optional<H26xPreset> h26xPreset;
    unsigned int vpXSpeed = 8;
    unsigned int gopsPerSegment = 1;

    bool operator==(const VideoQuality &) const;
};

/**
 * The channels.qualities.audio key.
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
 * The channels.qualities.clientBufferControl key.
 */
struct ClientBufferControl final
{
    std::optional<unsigned int> minBuffer;
    std::optional<unsigned int> extraBuffer;
    std::optional<unsigned int> initialBuffer;
    std::optional<unsigned int> seekBuffer;
    std::optional<unsigned int> minimumInitTime;

    bool operator==(const ClientBufferControl &) const;
};

/**
 * The channels.qualities key's elements.
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
 * The channels.dash key.
 */
struct Dash final
{
    unsigned int segmentDuration = 15000;
    bool expose = false;
    unsigned int preAvailabilityTime = 4000;

    bool operator==(const Dash &) const;
};

/**
 * The channels.ffmpeg key.
 */
struct ChannelFfmpeg final
{
    std::string filterZmq;

    bool operator==(const ChannelFfmpeg &) const;
};

/**
 * The history key.
 */
struct History final
{
    unsigned int historyLength = 90;
    std::string persistentStorage;

    bool operator==(const History &) const;
};

/**
 * The channels key's elements.
 */
struct Channel final
{
    Source source;
    std::vector<Quality> qualities;
    Dash dash;
    History history;
    ChannelFfmpeg ffmpeg;
    std::string name;
    std::string uid;

    bool operator==(const Channel &) const;

    /**
     * Determine whether the channel differs only by its UID and things that are calculated from it by default.
     */
    bool differsByUidOnly(const Channel &other) const;
};

/**
 * The network key.
 */
struct Network final
{
    uint16_t port = 8080;
    uint16_t publicPort = 0;
    std::vector<Server::Address> privateNetworks;
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
    std::vector<std::string> ephemeralWhenNotFound;

    bool operator==(const Http &) const;
};

/**
 * The directories key's elements.
 */
struct Directory final
{
    std::string localPath;
    std::string index;
    bool secure = false;
    bool ephemeral = false;
    size_t maxWritableSize = 0;

    bool operator==(const Directory &) const;
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
 * The features key.
 */
struct Features final
{
    bool channelIndex = true;

    bool operator==(const Features &) const;
};

/**
 * The separatedIngestSources key.
 */
struct SeparatedIngestSource final
{
    std::string url;
    std::vector<std::string> arguments;
    std::string path;
    size_t bufferSize = 1 << 24;
    size_t probeSize = 5000000; // Matches ffmpeg's default.

    bool operator==(const SeparatedIngestSource &) const;
};

/**
 * The exception that's thrown if there's an error parsing the configuration.
 */
class ParseException final : public std::runtime_error
{
public:
    ~ParseException() override;
    using runtime_error::runtime_error;
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

    /**
     * Perform initial construction with a lot of defaults already filled in, but still missing a lot of defaults and
     * the required values.
     *
     * This is useful as a starting point for fromJson and also allows default construction.
     */
    Root() = default;

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

#ifndef NDEBUG
    /**
     * Return a JSON representation of the configuration object as a string.
     */
    std::string toJson() const;
#endif // NDEBUG

    // The json this object was originally decoded from. If it was mutated afterwards, this will not be in sync.
    std::string jsonRepresentation;

    std::map<std::string, Channel> channels;
    std::map<std::string, Directory> directories;
    Network network;
    Http http;
    Log log;
    Features features;
    std::map<std::string, SeparatedIngestSource> separatedIngestSources;

    bool operator==(const Root &) const;

private:
    /**
     * Validate a loaded configuration.
     */
    void validate() const;
};

} // namespace Config

/// @}
