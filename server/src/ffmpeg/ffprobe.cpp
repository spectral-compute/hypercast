#include "ffprobe.hpp"

#include "Exceptions.hpp"

#include "configuration/configuration.hpp"
#include "util/asio.hpp"
#include "util/Event.hpp"
#include "util/json.hpp"
#include "util/subprocess.hpp"

#include <charconv>
#include <exception>
#include <numeric>
#include <utility>

/**
 * An entry in the probe cache.
 *
 * These entries are deleted once the last reference to them (via ProbeResult) is deleted.
 */
struct Ffmpeg::ProbeResult::CacheEntry final
{
    explicit CacheEntry(IOContext &ioc, std::vector<std::string> arguments) :
        arguments(std::move(arguments)), event(ioc)
    {
    }

    /**
     * The actual result.
     */
    MediaInfo::SourceInfo result;

    /**
     * The exception that occurred while trying to get the result, if any.
     */
    std::exception_ptr exception;

    /**
     * The arguments that go along with the URL for this result.
     */
    std::vector<std::string> arguments;

    /**
     * An event that gets notified when the result is filled in.
     */
    Event event;

    /**
     * Whether the result has been filled in by ffprobe.
     */
    bool filledIn = false;
};

namespace
{

/**
 * Parse a fraction from ffmpeg.
 *
 * @param string A string of the form "${numerator}/${denominator}" or "${numerator}".
 * @return A pair: numerator, denominator if RequireInteger is false.
 */
std::pair<unsigned int, unsigned int> parseFraction(std::string_view string)
{
    /* Gets an exception for a bad frame rate given by ffmpeg. */
    auto e = [&]() {
        return std::runtime_error("Bad fraction returned by ffprobe: \"" + std::string(string) + "\"\n");
    };

    /* Check that there's exactly one '/', and otherwise only the digits 0-9. */
    int slash = -1;
    for (size_t i = 0; i < string.size(); i++) {
        char c = string[i];
        if (c == '/') {
            if (slash >= 0) {
                throw e();
            }
            slash = (int)i;
        }
        else if (c < '0' || c > '9') {
            throw e();
        }
    }
    if (slash == 0 || slash >= (int)string.size() - 1) {
        throw e();
    }

    /* Parse the numerator and denominator. */
    unsigned int numerator = 0;
    unsigned int denominator = 1;

    if ((bool)std::from_chars(string.data(), string.data() + slash, numerator).ec ||
        (slash > 0 && (bool)std::from_chars(string.data() + slash + 1, string.data() + string.size(), denominator).ec))
    {
        throw e();
    }

    // Additional checking.
    if (denominator == 0) {
        throw e();
    }

    /* Normalize and check that we didn't get a fraction when we shouldn't. */
    unsigned int gcd = std::gcd(numerator, denominator);
    numerator /= gcd;
    denominator /= gcd;

    /* Done :) */
    return {numerator, denominator};
}

/**
 * Like parseFraction, but expects the fraction to actually be an integer.
 */
unsigned int parseFractionAsInteger(std::string_view string)
{
    auto [numerator, denominator] = parseFraction(string);
    if (denominator != 1) {
        throw std::runtime_error("Integer expected, but fraction returned by ffprobe: \"" + std::string(string) + "\"");
    }
    return numerator;
}

} // namespace

namespace MediaInfo
{

static void from_json(const nlohmann::json &j, VideoStreamInfo &out)
{
    auto [frameRateNumerator, frameRateDenominator] = parseFraction(j.at("r_frame_rate").get<std::string>());
    out = {
        .width = j.at("width").get<unsigned int>(),
        .height = j.at("height").get<unsigned int>(),
        .frameRateNumerator = frameRateNumerator,
        .frameRateDenominator = frameRateDenominator
    };
}

static void from_json(const nlohmann::json &j, AudioStreamInfo &out)
{
    out = {
        .sampleRate = parseFractionAsInteger(j.at("sample_rate").get<std::string>())
    };
}

} // namespace MediaInfo

Awaitable<Ffmpeg::ProbeResult> Ffmpeg::ffprobe(IOContext &ioc, std::string_view url, std::vector<std::string> arguments)
{
    /* Per-URL mutexes so we don't ffprobe the same thing in parallel. */
    static std::map<std::string, std::weak_ptr<ProbeResult::CacheEntry>> urlResults;

    /* Result caching. */
    // Create a mutex to the URL, so we don't probe it in parallel.
    std::string urlString(url);
    std::weak_ptr<ProbeResult::CacheEntry> &weakResult = urlResults[urlString]; // The coordinating reference.
    std::shared_ptr<ProbeResult::CacheEntry> result = weakResult.lock(); // Get the mutex if it exists.

    // If the cache entry already exists, wait for it to be filled in and return it.
    if (result) {
        // Make sure we've not got conflicting arguments.
        if (result->arguments != arguments) {
            throw InUseException("FFmpeg URL in use with different arguments.");
        }

        // Wait for the result to become ready.
        while (!result->filledIn) {
            co_await result->event.wait();
        }

        // Return the result wrapped in the wrapper cass.
        co_return ProbeResult(result);
    }

    // Create a cache entry in the map that deletes itself once it runs out of references.
    result = std::shared_ptr<ProbeResult::CacheEntry>
             (new ProbeResult::CacheEntry(ioc, std::move(arguments)),
              [urlString = std::move(urlString)](ProbeResult::CacheEntry *cacheEntry) {
        assert(urlResults.contains(urlString));
        assert(urlResults.at(urlString).expired());
        urlResults.erase(urlString);
        delete cacheEntry;
    });
    weakResult = result;

    /* Figure out how to run ffprobe. */
    std::vector<std::string_view> args;
    args.reserve(result->arguments.size() + 4);

    // The source's input arguments.
    for (std::string_view arg: result->arguments) {
        args.emplace_back(arg);
    }

    // The input itself.
    args.emplace_back(url);

    // The output arguments.
    for (const char *arg: {"-of", "json", "-show_streams"}) {
        args.emplace_back(arg);
    }

    /* Execute ffprobe and parse to JSON. */
    nlohmann::json j;
    try {
        j = Json::parse(co_await Subprocess::getStdout(ioc, "ffprobe", args));
    }
    catch (...) {
        // Store the exception in the result so it can be rethrown when accessed.
        result->exception = std::current_exception();
        result->filledIn = true;
        result->event.notifyAll(); // Alert everything else that's waiting on this result.
        co_return ProbeResult(result);
    }

    /* Build the result. */
    MediaInfo::SourceInfo &sourceInfo = result->result;

    // Parse through each stream looking for the best.
    bool foundDefaultVideo = false;
    bool foundDefaultAudio = false;
    for (const nlohmann::json &stream: j.at("streams")) {
        std::string type = stream.at("codec_type").get<std::string>();
        bool isDefault = stream.at("disposition").at("default").get<int>();

        if (type == "video") {
            // Prioritize which of multiple streams to use.
            if (foundDefaultVideo || (sourceInfo.video && !isDefault)) {
                continue;
            }
            foundDefaultVideo = isDefault;

            // Build the stream info object.
            sourceInfo.video = stream.get<MediaInfo::VideoStreamInfo>();
        }
        else if (type == "audio") {
            // Prioritize which of multiple streams to use.
            if (foundDefaultAudio || (sourceInfo.audio && !isDefault)) {
                continue;
            }
            foundDefaultAudio = isDefault;

            // Build the stream info object.
            sourceInfo.audio = stream.get<MediaInfo::AudioStreamInfo>();
        }
    }

    /* Done :) */
    result->filledIn = true;
    result->event.notifyAll(); // Alert everything else that's waiting on this result.
    co_return ProbeResult(result);
}

Awaitable<Ffmpeg::ProbeResult> Ffmpeg::ffprobe(IOContext &ioc, std::string_view url,
                                               std::span<const std::string> arguments)
{
    return ffprobe(ioc, url, std::vector(arguments.begin(), arguments.end()));
}

Awaitable<Ffmpeg::ProbeResult> Ffmpeg::ffprobe(IOContext &ioc, std::string_view url,
                                               std::span<const std::string_view> arguments)
{
    std::vector<std::string> argumentViews(arguments.size());
    for (size_t i = 0; i < arguments.size(); i++) {
        argumentViews[i] = arguments[i];
    }
    co_return co_await ffprobe(ioc, url, std::move(argumentViews));
}

Ffmpeg::ProbeResult::~ProbeResult() = default;

Ffmpeg::ProbeResult &Ffmpeg::ProbeResult::operator=(const ProbeResult &) = default;
Ffmpeg::ProbeResult &Ffmpeg::ProbeResult::operator=(ProbeResult &&) = default;

Ffmpeg::ProbeResult::operator const MediaInfo::SourceInfo &() const
{
    if (cacheEntry->exception) {
        std::rethrow_exception(cacheEntry->exception);
    }
    return cacheEntry->result;
}
