#include "ProbeResource.hpp"

#include "configuration/configuration.hpp"
#include "ffmpeg/Exceptions.hpp"
#include "ffmpeg/ffprobe.hpp"
#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/json.hpp"

namespace
{

/**
 * A source to probe.
 */
struct Source final
{
    std::string url;
    std::vector<std::string> arguments;
};

/**
 * The result of a probe.
 */
struct ProbeResult final
{
    /**
     * The source info.
     */
    MediaInfo::SourceInfo sourceInfo;

    /**
     * Whether the source is in use.
     */
    bool inUse = false;
};

void from_json(const nlohmann::json &j, Source &out)
{
    Json::ObjectDeserializer d(j);
    d(out.url, "url", true);
    d(out.arguments, "arguments");
    d();
}

Awaitable<void> probeSource(IOContext &ioc, ProbeResult &result, const std::set<std::string> &inUseUrls,
                            const Source &source)
{
    /* Try to probe the source to see what it contains. */
    try {
        result = { co_await Ffmpeg::ffprobe(ioc, source.url, source.arguments), inUseUrls.contains(source.url) };
    }

    /* Handle the case where the URL is in use with different arguments. */
    catch (const Ffmpeg::InUseException &) {
        throw Server::Error(Server::ErrorKind::Conflict);
    }

    /* In case of exception, the source probably isn't connected, or is otherwise not usable. */
    catch (...) {}
}

} // namespace

namespace MediaInfo
{

static void to_json(nlohmann::json &j, const VideoStreamInfo &in)
{
    j["width"] = in.width;
    j["height"] = in.height;
    j["frameRate"] = { in.frameRateNumerator, in.frameRateDenominator };
}

static void to_json(nlohmann::json &j, const AudioStreamInfo &in)
{
    j["sampleRate"] = in.sampleRate;
}

} // namespace MediaInfo

namespace
{

void to_json(nlohmann::json &j, const ProbeResult &in)
{
    /* The input is not usable if there's no video. */
    if (!in.sourceInfo.video) {
        return;
    }

    /* Fill in the result JSON object. */
    j["video"] = *in.sourceInfo.video;
    if (in.sourceInfo.audio) {
        j["audio"] = *in.sourceInfo.audio;
    }
    j["inUse"] = in.inUse;
}

} // namespace

Api::ProbeResource::~ProbeResource() = default;

Awaitable<void> Api::ProbeResource::postAsync(Server::Response &response, Server::Request &request)
{
    try {
        /* Get all the sources and somewhere to put the result. */
        auto sources = Json::parse(co_await request.readAllAsString()).get<std::vector<Source>>();
        std::vector<ProbeResult> result(sources.size());

        /* Run probeSource on each source in parallel. */
        std::vector<Awaitable<void>> awaitables;
        awaitables.reserve(sources.size());
        for (size_t i = 0; i < sources.size(); i++) {
            awaitables.emplace_back(probeSource(ioc, result[i], inUseUrls, sources[i]));
        }
        co_await awaitTree(awaitables);

        /* Send the result back. */
        response.setMimeType("application/json");
        response << Json::dump(result);
    }
    catch (const nlohmann::json::exception &e) {
        /* Bad request because the body is not a valid JSON object. */
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
    catch (const Json::ObjectDeserializer::Exception &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
}

size_t Api::ProbeResource::getMaxPostRequestLength() const noexcept
{
    return 1 << 12; // 4 kiB.
}
