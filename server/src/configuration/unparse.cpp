#ifndef NDEBUG

#include "configuration.hpp"

#include "util/debug.hpp"
#include "util/json.hpp"

#include <cassert>

namespace Config
{

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Source &in)
{
    j["url"] = in.url;
    j["arguments"] = in.arguments;
    j["loop"] = in.loop;
    j["timestamp"] = in.timestamp;
    j["latency"] = *in.latency;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const FrameRate &in)
{
    assert(in.type == FrameRate::fps);
    j = { in.numerator, in.denominator };
}

namespace
{

/// @ingroup configuration_implementation
std::string toString(Codec::VideoCodec in)
{
    switch (in) {
        case Codec::VideoCodec::h264: return "h264";
        case Codec::VideoCodec::h265: return "h265";
        case Codec::VideoCodec::vp8: return "vp8";
        case Codec::VideoCodec::vp9: return "vp9";
        case Codec::VideoCodec::av1: return "av1";
    }
    unreachable();
}

/// @ingroup configuration_implementation
std::string toString(H26xPreset in)
{
    switch (in) {
        case H26xPreset::ultrafast: return "ultrafast";
        case H26xPreset::superfast: return "superfast";
        case H26xPreset::veryfast: return "veryfast";
        case H26xPreset::faster: return "faster";
        case H26xPreset::fast: return "fast";
        case H26xPreset::medium: return "medium";
        case H26xPreset::slow: return "slow";
        case H26xPreset::slower: return "slower";
        case H26xPreset::veryslow: return "veryslow";
        case H26xPreset::placebo: return "placebo" ;
    }
    unreachable();
}

} // namespace

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const VideoQuality &in)
{
    j["width"] = *in.width;
    j["height"] = *in.height;
    j["frameRate"] = in.frameRate;
    j["bitrate"] = *in.bitrate;
    j["minBitrate"] = *in.minBitrate;
    j["crf"] = in.crf;
    j["rateControlBufferLength"] = *in.rateControlBufferLength;
    j["codec"] = toString(in.codec);
    j["h26xPreset"] = toString(*in.h26xPreset);
    j["vpXSpeed"] = in.vpXSpeed;
    j["gop"] = *in.gop;
}

namespace
{

/// @ingroup configuration_implementation
std::string toString(Codec::AudioCodec in)
{
    switch (in) {
        case Codec::AudioCodec::none: return "none";
        case Codec::AudioCodec::aac: return "aac";
        case Codec::AudioCodec::opus: return "opus";
    }
    unreachable();
}

} // namespace

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const AudioQuality &in)
{
    j["bitrate"] = in.bitrate;
    j["codec"] = toString(in.codec);
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const ClientBufferControl &in)
{
    j["minBuffer"] = *in.minBuffer;
    j["extraBuffer"] = *in.extraBuffer;
    j["initialBuffer"] = *in.initialBuffer;
    j["seekBuffer"] = *in.seekBuffer;
    j["minimumInitTime"] = *in.minimumInitTime;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Quality &in)
{
    j["video"] = in.video;
    j["audio"] = in.audio;
    j["targetLatency"] = in.targetLatency;
    j["minInterleaveRate"] = *in.minInterleaveRate;
    j["minInterleaveWindow"] = *in.minInterleaveWindow;
    j["interleaveTimestampInterval"] = in.interleaveTimestampInterval;
    j["clientBufferControl"] = in.clientBufferControl;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Dash &in)
{
    j["segmentDuration"] = in.segmentDuration;
    j["expose"] = in.expose;
    j["preAvailabilityTime"] = in.preAvailabilityTime;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const History &in)
{
    j["historyLength"] = in.historyLength;
    j["persistentStorage"] = in.persistentStorage;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Channel &in)
{
    j["source"] = in.source;
    j["qualities"] = in.qualities;
    j["dash"] = in.dash;
    j["history"] = in.history;
    j["name"] = in.name;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Directory &in)
{
    j["localPath"] = in.localPath;
    j["index"] = in.index;
    j["secure"] = in.secure;
    j["ephemeral"] = in.ephemeral;
    j["maxWritableSize"] = in.maxWritableSize;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Network &in)
{
    j["port"] = in.port;
    j["publicPort"] = in.publicPort;
    j["privateNetworks"] = in.privateNetworks;
    j["transitLatency"] = in.transitLatency;
    j["transitJitter"] = in.transitJitter;
    j["transitBufferSize"] = in.transitBufferSize;
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Http &in)
{
    j["origin"] = *in.origin;
    j["cacheNonLiveTime"] = in.cacheNonLiveTime;
    j["ephemeralWhenNotFound"] = in.ephemeralWhenNotFound;
}

namespace
{

/// @ingroup configuration_implementation
std::string toString(::Log::Level in)
{
    switch (in) {
        case ::Log::Level::debug: return "debug";
        case ::Log::Level::info: return "info";
        case ::Log::Level::warning: return "warning";
        case ::Log::Level::error: return "error";
        case ::Log::Level::fatal: return "fatal";
    }
    unreachable();
}

} // namespace

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Log &in)
{
    j["path"] = in.path;
    j["print"] = *in.print;
    j["level"] = toString(in.level);
}

/// @ingroup configuration_implementation
static void to_json(nlohmann::json &j, const Features &in)
{
    j["channelIndex"] = in.channelIndex;
}

} // namespace Config

std::string Config::Root::toJson() const
{
    nlohmann::json j;
    j["channels"] = channels;
    j["directories"] = directories;
    j["network"] = network;
    j["http"] = http;
    j["log"] = log;
    j["features"] = features;
    return Json::dump(j);
}

#endif // NDEBUG
