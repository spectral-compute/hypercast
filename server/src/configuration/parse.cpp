#include "configuration.hpp"

#include "util/json.hpp"

/// @addtogroup configuration
/// @{
/// @defgroup configuration_implementation Implementation
/// @}

/// @addtogroup configuration_implementation
/// @{

using namespace std::string_literals;

namespace
{

/**
 * Get an exception for a configuration parse error.
 *
 * @param message The what() message for the exception.
 */
[[nodiscard]] auto ParseException(std::string_view message)
{
    return std::runtime_error("Error parsing configuration: " + std::string(message));
}

/**
 * Get an exception for a configuration parse error at a given key.
 *
 * @param key The key in the configuration that is the problem.
 * @param message The reason this key is a problem.
 */
[[nodiscard]] auto ParseException(const char *key, std::string_view message)
{
    std::string messageStr(message);
    return std::runtime_error(key ? "Error parsing configuration at key \""s + key + "\": " + messageStr :
                                    ("Error parsing configuration at root: " + messageStr));
}

} // namespace

/// @}

// It's annoying that these have to be in the right namespace, and thus end up mis-documented.
namespace Config
{

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Source &out)
{
    Json::ObjectDeserializer d(j, "source");
    d(out.url, "url");
    d(out.arguments, "arguments");
    d(out.loop, "loop");
    d(out.timestamp, "timestamp");
    d(out.latency, "latency");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, FrameRate &out)
{
    constexpr const char *key = "qualities.video.frameRate";

    /* Handle the case where it's a string. */
    if (j.is_string()) {
        std::string value = j.get<std::string>();
        if (value == "half") {
            out.type = FrameRate::fraction;
            out.numerator = 1;
            out.denominator = 2;
        }
        else if (value == "half+") {
            out.type = FrameRate::fraction23;
            out.numerator = 1;
            out.denominator = 2;
        }
        else {
            throw ParseException(key, "Unknown string value \"" + value + "\".");
        }
        return;
    }

    /* Otherwise, it should be an array with two values. */
    if (!j.is_array()) {
        throw ParseException(key, "Value is not a string or array.");
    }
    if (j.size() != 2) {
        throw ParseException(key, "Value is an array, but not of length 2.");
    }

    /* Extract the array into a pair of integers. */
    try {
        out.numerator = j[0].get<int>();
        out.denominator = j[1].get<int>();
    }
    catch (const nlohmann::json::type_error &e) {
        throw ParseException(key, "Array element has incorrect type: "s + e.what() + ".");
    }
    catch (const nlohmann::json::out_of_range &e) {
        throw ParseException(key, "Array element is out of range: "s + e.what() + ".");
    }
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, VideoQuality &out)
{
    Json::ObjectDeserializer d(j, "qualities.video");
    d(out.width, "width");
    d(out.height, "height");
    d(out.frameRate, "frameRate");
    d(out.bitrate, "bitrate");
    d(out.minBitrate, "minBitrate");
    d(out.crf, "crf");
    d(out.rateControlBufferLength, "rateControlBufferLength");
    d(out.codec, "codec", {
        { Codec::VideoCodec::h264, "h264" },
        { Codec::VideoCodec::h265, "h265" },
        { Codec::VideoCodec::vp8, "vp8" },
        { Codec::VideoCodec::vp9, "vp9" },
        { Codec::VideoCodec::av1, "av1" }
    });
    d(out.h26xPreset, "h26xPreset", {
        { H26xPreset::ultrafast, "ultrafast" },
        { H26xPreset::superfast, "superfast" },
        { H26xPreset::veryfast, "veryfast" },
        { H26xPreset::faster, "faster" },
        { H26xPreset::fast, "fast" },
        { H26xPreset::medium, "medium" },
        { H26xPreset::slow, "slow" },
        { H26xPreset::slower, "slower" },
        { H26xPreset::veryslow, "veryslow" },
        { H26xPreset::placebo, "placebo "}
    });
    d(out.vpXSpeed, "vpXSpeed");
    d(out.gop, "gop");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, AudioQuality &out)
{
    Json::ObjectDeserializer d(j, "qualities.audio");
    d(out.bitrate, "bitrate");
    d(out.codec, "codec", {
        { Codec::AudioCodec::none, "none" },
        { Codec::AudioCodec::aac, "aac" },
        { Codec::AudioCodec::opus, "opus" }
    });
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, ClientBufferControl &out)
{
    Json::ObjectDeserializer d(j, "qualities.clientBufferControl");
    d(out.extraBuffer, "extraBuffer");
    d(out.initialBuffer, "initialBuffer");
    d(out.seekBuffer, "seekBuffer");
    d(out.minimumInitTime, "minimumInitTime");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Quality &out)
{
    Json::ObjectDeserializer d(j, "qualities");
    d(out.video, "video");
    d(out.audio, "audio");
    d(out.targetLatency, "targetLatency");
    d(out.minInterleaveRate, "minInterleaveRate");
    d(out.minInterleaveWindow, "minInterleaveWindow");
    d(out.interleaveTimestampInterval, "interleaveTimestampInterval");
    d(out.clientBufferControl, "clientBufferControl");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Dash &out)
{
    Json::ObjectDeserializer d(j, "dash");
    d(out.segmentDuration, "segmentDuration");
    d(out.expose, "expose");
    d(out.preAvailabilityTime, "preAvailabilityTime");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Network &out)
{
    Json::ObjectDeserializer d(j, "network");
    d(out.port, "port");
    d(out.transitLatency, "transitLatency");
    d(out.transitJitter, "transitJitter");
    d(out.transitBufferSize, "transitBufferSize");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Http &out)
{
    Json::ObjectDeserializer d(j, "http");
    d(out.origin, "origin");
    d(out.cacheNonLiveTime, "cacheNonLiveTime");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Directory &out)
{
    /* Deserialize the short-hand form. */
    if (j.is_string()) {
        out.localPath = j.get<std::string>();
        return;
    }

    /* Deserialize the long form. */
    Json::ObjectDeserializer d(j, "paths.directories");
    d(out.localPath, "qualities");
    d(out.index, "http");
    d(out.secure, "paths");
    d(out.ephemeral, "history");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Paths &out)
{
    Json::ObjectDeserializer d(j, "paths");
    d(out.liveInfo, "liveInfo");
    d(out.liveStream, "liveStream");
    d(out.directories, "directories");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, History &out)
{
    Json::ObjectDeserializer d(j, "history");
    d(out.historyLength, "historyLength");
    d();
}

/// @ingroup implementation
static void from_json(const nlohmann::json &j, Log &out)
{
    Json::ObjectDeserializer d(j, "log");
    d(out.path, "path");
    d(out.print, "print");
    d(out.level, "level", {
        { ::Log::Level::debug, "debug" },
        { ::Log::Level::info, "info" },
        { ::Log::Level::warning, "warning" },
        { ::Log::Level::error, "error" },
        { ::Log::Level::fatal, "fatal" }
    });
    d();
}

} // namespace Config

#ifndef WITH_TESTING
Config::Root::~Root() = default; // Actually somewhat complicated.
#endif // WITH_TESTING

Config::Root Config::Root::fromJson(std::string_view jsonString)
{
    /* Try to parse the string into a JSON value. */
    nlohmann::json j;
    try {
        j = Json::parse(jsonString, true);
    }
    catch (const nlohmann::json::parse_error &e) {
        throw ParseException(e.what());
    }

    /* Deserialize the JSON value. */
    Config::Root root;

    // Any nlohmann::json exceptions this would raise should be handled by the from_json() implementations above.
    try {
        Json::ObjectDeserializer d(j);
        d(root.source, "source", true);
        d(root.qualities, "qualities");
        d(root.dash, "dash");
        d(root.network, "network");
        d(root.http, "http");
        d(root.paths, "paths");
        d(root.history, "history");
        d(root.log, "log");
        d();
    }
    catch (const Json::ObjectDeserializer::Exception &e) {
        throw ParseException(e.getKey() ? e.getKey()->c_str() : nullptr, e.getMessage());
    }

    /* Finish off. */
    root.validate();
    return root;
}
