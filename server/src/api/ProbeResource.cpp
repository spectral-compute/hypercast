#include "ProbeResource.hpp"

#include "configuration/configuration.hpp"
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

void from_json(const nlohmann::json &j, Source &out)
{
    Json::ObjectDeserializer d(j);
    d(out.url, "url", true);
    d(out.arguments, "arguments");
    d();
}

Awaitable<void> probeSource(IOContext &ioc, MediaInfo::SourceInfo &result, const Source &source)
{
    /* Try to probe the source to see what it contains. */
    try {
        result = co_await Ffmpeg::ffprobe(ioc, { .url = source.url, .arguments = source.arguments });
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

static void to_json(nlohmann::json &j, const SourceInfo &in)
{
    if (in.video) {
        j["video"] = *in.video;
    }
    if (in.audio) {
        j["audio"] = *in.audio;
    }
}

} // namespace MediaInfo

Api::ProbeResource::~ProbeResource() = default;

Awaitable<void> Api::ProbeResource::getAsync(Server::Response &response, Server::Request &request)
{
    response.setCacheKind(Server::CacheKind::none);

    try {
        /* Get all the sources and somewhere to put the result. */
        auto sources = Json::parse(co_await request.readAllAsString()).get<std::vector<Source>>();
        std::vector<MediaInfo::SourceInfo> result(sources.size());

        /* Run probeSource on each source in parallel. */
        std::vector<Awaitable<void>> awaitables;
        awaitables.reserve(sources.size());
        for (size_t i = 0; i < sources.size(); i++) {
            awaitables.emplace_back(probeSource(ioc, result[i], sources[i]));
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

size_t Api::ProbeResource::getMaxGetRequestLength() const noexcept
{
    return 1 << 12; // 4 kiB.
}
