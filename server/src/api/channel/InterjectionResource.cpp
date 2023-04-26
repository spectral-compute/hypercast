#include "InterjectionResource.hpp"

#include "dash/DashResources.hpp"
#include "ffmpeg/Process.hpp"
#include "server/Error.hpp"
#include "server/Request.hpp"
#include "util/asio.hpp"
#include "util/json.hpp"

namespace
{

/**
 * The server-side interjection request object.
 *
 * Some of these fields match those in API.InterjectionRequest in the client. Those fields are documented there.
 */
struct RequestObject final
{
    /**
     * The length of the interjections, in ms.
     *
     * This sets `maxPlayEndPts - minPlayStartPts`.
     */
    unsigned int duration;

    /**
     * How much flexibility there is in the duration.
     *
     * This sets `maxPlayEndPts - minPlayEndPts`.
     */
    unsigned int durationWindow = 2000;

    /**
     * How long between the current position in the live stream and when the interjections should start playing, in ms.
     *
     * This sets `minPlayStartPts`.
     */
    unsigned int delay = 5000;

    /**
     * How long the window for starting to play interjections is.
     *
     * This sets `maxPlayStartPts - minPlayStartPts`. By default, it's chosen to set `maxPlayStartPts == minPlayEndPts`.
     */
    std::optional<unsigned int> delayWindow;

    /**
     * Whether to blank the stream while the interjections should be playing.
     */
    bool blank = true;

    /**
     * Set to !blank by default.
     */
    std::optional<bool> mainStreamFallbackInitial;

    /**
     * Set to mainStreamFallbackInitial by default.
     */
    std::optional<bool> mainStreamFallbackSubsequent;

    std::string setUrl;
    unsigned int maxSelectTime = 3000;
    unsigned int minSelectTime = 1000;
    std::optional<unsigned int> playingInterjectionPriorityTime = 0;
    std::optional<unsigned int> interjectionSelectionPriorityTime = 0;
    unsigned int mainStreamWarmUpTime = 3000;
    nlohmann::json other;
};

void from_json(const nlohmann::json &j, RequestObject &out)
{
    Json::ObjectDeserializer d(j);

    d(out.duration, "duration", true);
    d(out.durationWindow, "durationWindow");
    d(out.delay, "delay");
    d(out.delayWindow, "delayWindow");
    d(out.blank, "blank");
    d(out.setUrl, "setUrl", true);
    d(out.maxSelectTime, "maxSelectTime");
    d(out.minSelectTime, "minSelectTime");
    d(out.mainStreamFallbackInitial, "mainStreamFallbackInitial");
    d(out.mainStreamFallbackSubsequent, "mainStreamFallbackSubsequent");
    d(out.playingInterjectionPriorityTime, "playingInterjectionPriorityTime");
    d(out.interjectionSelectionPriorityTime, "interjectionSelectionPriorityTime");
    d(out.mainStreamWarmUpTime, "mainStreamWarmUpTime");

    {
        auto it = d("other");
        if (it != j.end()) {
            out.other = *it;
        }
    }

    d();
}

/**
 * Makes sure the request is sane.
 */
void validateRequest(const RequestObject &requestObject)
{
    if (requestObject.duration < requestObject.durationWindow) {
        throw Server::Error(Server::ErrorKind::BadRequest, "Duration window is longer than the duration.");
    }
    if (requestObject.maxSelectTime <= requestObject.minSelectTime) {
        throw Server::Error(Server::ErrorKind::BadRequest, "Select time window is empty.");
    }
}

/**
 * Fills in defaults for a request.
 */
void setRequestDefaults(RequestObject &requestObject)
{
    if (!requestObject.delayWindow) {
        requestObject.delayWindow = requestObject.duration - requestObject.durationWindow;
    }
    if (!requestObject.mainStreamFallbackInitial) {
        requestObject.mainStreamFallbackInitial = !requestObject.blank;
    }
    if (!requestObject.mainStreamFallbackSubsequent) {
        requestObject.mainStreamFallbackSubsequent = *requestObject.mainStreamFallbackInitial;
    }
}

/**
 * Convert an optional to JSON null if not set, otherwise to a JSON value.
 */
template <typename T>
nlohmann::json orNull(const std::optional<T> &value)
{
    return value ? *value : nlohmann::json();
}

} // namespace

Api::Channel::InterjectionResource::~InterjectionResource() = default;

Awaitable<void> Api::Channel::InterjectionResource::postAsync(Server::Response &, Server::Request &request)
{
    try {
        /* Parse the request. */
        RequestObject requestObject = Json::parse(co_await request.readAllAsString()).get<RequestObject>();
        validateRequest(requestObject);
        setRequestDefaults(requestObject);

        /* Create the InterjectionRequest object. */
        int64_t pts = (int64_t)std::round((co_await ffmpegProcess.getPts()).getValueInSeconds() * 1000);
        nlohmann::json interjectionRequest = {
            { "setUrl", requestObject.setUrl },
            { "maxSelectTime", requestObject.maxSelectTime },
            { "minSelectTime", requestObject.minSelectTime },
            { "minPlayStartPts", pts + requestObject.delay },
            { "maxPlayStartPts", pts + requestObject.delay + *requestObject.delayWindow },
            { "minPlayEndPts", pts + requestObject.duration - requestObject.durationWindow },
            { "maxPlayEndPts", pts + requestObject.duration },
            { "mainStreamFallbackInitial", *requestObject.mainStreamFallbackInitial },
            { "mainStreamFallbackSubsequent", *requestObject.mainStreamFallbackSubsequent },
            { "playingInterjectionPriorityTime", orNull(requestObject.playingInterjectionPriorityTime) },
            { "interjectionSelectionPriorityTime", orNull(requestObject.interjectionSelectionPriorityTime) },
            { "mainStreamWarmUpTime", requestObject.mainStreamWarmUpTime }
        };

        if (!requestObject.other.is_null()) {
            interjectionRequest["other"] = std::move(requestObject.other);
        }

        /* Send the interjection request to the clients. */
        channel.addJsonObjectControlChunk(std::move(interjectionRequest), "interject");

        /* Blank the stream. */
        // TODO
    }
    catch (const nlohmann::json::exception &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
    catch (const Json::ObjectDeserializer::Exception &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
}

size_t Api::Channel::InterjectionResource::getMaxPostRequestLength() const noexcept
{
    return 16384;
}
