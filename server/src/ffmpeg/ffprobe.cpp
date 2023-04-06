#include "ffprobe.hpp"

#include "configuration/configuration.hpp"
#include "util/asio.hpp"
#include "util/json.hpp"
#include "util/Mutex.hpp"
#include "util/RaiiFn.hpp"
#include "util/subprocess.hpp"

#include <charconv>
#include <numeric>
#include <utility>

namespace
{

/**
 * Per-URL mutexes so we don't ffprobe the same thing in parallel.
 *
 * Eventually, the plan is to rely on this less by maintaining a monitor coroutine for sources we care about in
 * Api::ProbeResource. This will still be necessary then in case something not in the monitor list is probed.
 */
std::map<std::string, std::weak_ptr<Mutex>> urlMutexes;

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

Awaitable<MediaInfo::SourceInfo> Ffmpeg::ffprobe(IOContext &ioc, std::string_view url,
                                                 std::span<const std::string_view> arguments)
{
    /* Figure out how to run ffprobe. */
    std::vector<std::string_view> args;
    args.reserve(arguments.size() + 4);

    // The source's input arguments.
    for (std::string_view arg: arguments) {
        args.emplace_back(arg);
    }

    // The input itself.
    args.emplace_back(url);

    // The output arguments.
    for (const char *arg: {"-of", "json", "-show_streams"}) {
        args.emplace_back(arg);
    }

    /* Run ffprobe with a mutex for the URL. */
    std::string ffprobeResult;
    {
        // Create a mutex to the URL, so we don't probe it in parallel.
        std::string urlString(url);
        std::weak_ptr<Mutex> &weakMutex = urlMutexes[urlString]; // The coordinating reference.
        std::shared_ptr<Mutex> mutex = weakMutex.lock(); // Get the mutex if it exists.
        if (!mutex) {
            // Create the mutex if it doesn't, and share it with anything that wnats it next.
            mutex = std::make_shared<Mutex>(ioc);
            weakMutex = mutex;
        }

        // Make sure the mutex gets deleted if no one is using it once we're done.
        Util::RaiiFn cleanup([&mutex, &weakMutex, &urlString]() {
            mutex.reset();
            if (weakMutex.expired()) {
                urlMutexes.erase(urlString);
            }
        });

        // Execute ffmpeg with the mutex locked.
        Mutex::LockGuard lock = co_await mutex->lockGuard();
        ffprobeResult = co_await Subprocess::getStdout(ioc, "ffprobe", args);
    }

    /* Parse to JSON. */
    nlohmann::json j = Json::parse(ffprobeResult);

    /* Build the result. */
    MediaInfo::SourceInfo result;

    // Parse through each stream looking for the best.
    bool foundDefaultVideo = false;
    bool foundDefaultAudio = false;
    for (const nlohmann::json &stream: j.at("streams")) {
        std::string type = stream.at("codec_type").get<std::string>();
        bool isDefault = stream.at("disposition").at("default").get<int>();

        if (type == "video") {
            // Prioritize which of multiple streams to use.
            if (foundDefaultVideo || (result.video && !isDefault)) {
                continue;
            }
            foundDefaultVideo = isDefault;

            // Build the stream info object.
            result.video = stream.get<MediaInfo::VideoStreamInfo>();
        }
        else if (type == "audio") {
            // Prioritize which of multiple streams to use.
            if (foundDefaultAudio || (result.audio && !isDefault)) {
                continue;
            }
            foundDefaultAudio = isDefault;

            // Build the stream info object.
            result.audio = stream.get<MediaInfo::AudioStreamInfo>();
        }
    }

    /* Done :) */
    co_return result;
}

Awaitable<MediaInfo::SourceInfo> Ffmpeg::ffprobe(IOContext &ioc, std::string_view url,
                                                 std::span<const std::string> arguments)
{
    std::vector<std::string_view> argumentViews(arguments.size());
    for (size_t i = 0; i < arguments.size(); i++) {
        argumentViews[i] = arguments[i];
    }
    co_return co_await ffprobe(ioc, url, argumentViews);
}
