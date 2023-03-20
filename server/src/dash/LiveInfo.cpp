#include "configuration/configuration.hpp"
#include "server/Path.hpp"

#include "util/debug.hpp"
#include "util/json.hpp"

namespace Codec
{

static void to_json(nlohmann::json &j, const VideoCodec &in)
{
    switch (in) {
        case VideoCodec::h264: j = "h264"; break;
        case VideoCodec::h265: j = "h265"; break;
        case VideoCodec::vp8: j = "vp8"; break;
        case VideoCodec::vp9: j = "vp9"; break;
        case VideoCodec::av1: j = "av1"; break;
    }
}

static void to_json(nlohmann::json &j, const AudioCodec &in)
{
    switch (in) {
        case AudioCodec::none: unreachable();
        case AudioCodec::aac: j = "aac"; break;
        case AudioCodec::opus: j = "opus"; break;
    }
}

} // namespace Codec

namespace
{

/**
 * Get the angle descriptor.
 *
 * This is only here temporarily, since I'm planning to get rid of multi-angle support. If a customer requires this, it
 * can most easily be implemented with multiple instances of the server. The use case of sharing audio and video streams
 * doesn't work well due to the audio streams getting stuck in the CDN buffer.
 *
 * @param basePath The base path for the live streaming directory.
 */
nlohmann::json getAngle(const Server::Path &basePath)
{
    return {
        { "name", "Main" },
        { "path", basePath / "manifest.mpd" }
    };
}

/**
 * Get a video configuration.
 *
 * This is due to be merged into a single quality descriptor combining audio, video, and client buffer information.
 */
nlohmann::json getVideoConfig(const Config::Quality &q)
{
    return {
        { "codec", q.video.codec },
        { "bitrate", *q.video.bitrate },
        { "width", *q.video.width },
        { "height", *q.video.height },
        {
            "bufferCtrl",
            {
                { "extraBuffer", *q.clientBufferControl.extraBuffer },
                { "initialBuffer", *q.clientBufferControl.initialBuffer },
                { "seekBuffer", *q.clientBufferControl.seekBuffer },
                { "minimumInitTime", *q.clientBufferControl.minimumInitTime }
            }
        }
    };
}

nlohmann::json getVideoConfigs(const Config::Root &config)
{
    nlohmann::json j = nlohmann::json::array();
    for (const Config::Quality &q: config.qualities) {
        j.push_back(getVideoConfig(q));
    }
    return j;
}

nlohmann::json getAudioConfig(const Config::AudioQuality &q)
{
    return {
        { "codec", q.codec },
        { "bitrate", q.bitrate },
    };
}

nlohmann::json getAudioConfigs(const Config::Root &config)
{
    nlohmann::json j = nlohmann::json::array();
    for (const Config::Quality &q: config.qualities) {
        if (q.audio) {
            j.push_back(getAudioConfig(q.audio));
        }
    }
    return j;
}

nlohmann::json getAvMap(const Config::Root &config)
{
    nlohmann::json j = nlohmann::json::array();
    size_t videoIndex = 0;
    size_t audioIndex = config.qualities.size();
    for (const Config::Quality &q: config.qualities) {
        j.push_back({ videoIndex++, q.audio ? audioIndex++ : (nlohmann::json)nullptr });
    }
    return j;
}

} // namespace

namespace Dash
{

std::string getLiveInfo(const Config::Root &config, const Server::Path &basePath)
{
    return Json::dump({
        { "angles", nlohmann::json::array({ getAngle(basePath) }) },
        { "segmentDuration", config.dash.segmentDuration },
        { "segmentPreavailability", config.dash.preAvailabilityTime },
        { "videoConfigs", getVideoConfigs(config) },
        { "audioConfigs", getAudioConfigs(config) },
        { "avMap", getAvMap(config) }
    });
}

} // namespace Dash