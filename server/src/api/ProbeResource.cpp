#include "ProbeResource.hpp"

#include "configuration/configuration.hpp"
#include "ffmpeg/Exceptions.hpp"
#include "ffmpeg/ffprobe.hpp"
#include "ffmpeg/ProbeCache.hpp"
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

Awaitable<void> probeSource(IOContext &ioc, ProbeResult &result, const Ffmpeg::ProbeCache &configProbes,
                            const Source &source)
{
    /* See if the source is in use, and if so return from the cache. */
    if (const MediaInfo::SourceInfo *cachedResult = configProbes[source.url, source.arguments]) {
        result = { *cachedResult, true };
        co_return;
    }

    /* Don't probe a URL that's in use, even if the arguments differ. */
    if (configProbes.contains(source.url)) {
        throw Server::Error(Server::ErrorKind::Conflict);
    }

    /* Try to probe the source to see what it contains. */
    try {
        result = { co_await Ffmpeg::ffprobe(ioc, source.url, source.arguments) };
    }

    /* Handle the case where the URL is in use with different arguments. */
    catch (const Ffmpeg::InUseException &) {
        throw Server::Error(Server::ErrorKind::Conflict);
    }

    /* In case of exception, the source probably isn't connected, or is otherwise not usable. */
    catch (...) {}
}

Awaitable<void> awaitTree(std::span<Awaitable<void>> awaitables)
{
    if (awaitables.empty()) {}
    else if (awaitables.size() == 1) {
        co_await std::move(awaitables[0]);
    }
    else {
        co_await (awaitTree(awaitables.subspan(0, awaitables.size() / 2)) &&
                  awaitTree(awaitables.subspan(awaitables.size() / 2)));
    }
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
            awaitables.emplace_back(probeSource(ioc, result[i], configProbes, sources[i]));
        }
        co_await awaitTree(awaitables);

        /* Send the result back. */
        response.setMimeType("application/json");
        response << Json::dump(result);
    }
    catch (const nlohmann::json::exception &) {
        /* Bad request because the body is not a valid JSON object. */
        throw Server::Error(Server::ErrorKind::BadRequest);
    }
}

size_t Api::ProbeResource::getMaxPostRequestLength() const noexcept
{
    return 1 << 12; // 4 kiB.
}
