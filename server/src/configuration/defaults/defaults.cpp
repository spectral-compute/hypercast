#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"
#include "ffmpeg/ffprobe.hpp"
#include "server/Path.hpp"

namespace
{

/**
 * Generate a unique ID.
 *
 * This is useful for URLs that might otherwise conflict with stale versions in a cache.
 */
std::string generateUid()
{
    constexpr const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    constexpr int alphabetSize = sizeof(alphabet) - 1; // The -1 removes the null terminator.

    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>
                  (std::chrono::system_clock::now().time_since_epoch()).count();

    std::string result;
    while (ms > 0) {
        result += alphabet[ms % alphabetSize];
        ms /= alphabetSize;
    }
    return result;
}

/**
 * Replace characters in a path to include only safe characters and no path separators.
 */
std::string sanitizePathToFilename(std::string_view path)
{
    std::string result;
    for (char c: path) {
        result += ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-') ?
                  c : '_';
    }
    return result;
}

} // namespace

namespace Config
{

Awaitable<void>
fillInQualitiesFromFfprobe(std::vector<Quality> &qualities, const Source &source, const ProbeFunction &probe);

void fillInQuality(Config::Quality &q, const Root &config, const Channel &channel);
void fillInCompute(Root &config);

} // namespace Config

Awaitable<void> Config::fillInDefaults(const ProbeFunction &probe, Root &config)
{
    // TODO: This is likely no longer correct, since the ffprobeage will need to be changed to cope with
    //       multiple input ports.
    /* Fill in some simple defaults. */
    if (!config.log.print) {
        config.log.print = config.log.path.empty(); // By default, print if and only if we're not logging to a file.
    }

    /* Fill in the channels. */
    for (auto &[path, channel]: config.channels) {
        // If there are no qualities, add one for fillInQualitiesFromFfprobe to fill in.
        if (channel.qualities.empty()) {
            channel.qualities.emplace_back();
        }

        // Fill in the information we get from ffprobe. This is done first because a lot of other stuff is based on
        // this.
        co_await fillInQualitiesFromFfprobe(channel.qualities, channel.source, probe);

        // Fill in prerequisites to the latency tracker.
        if (!channel.source.latency) {
            channel.source.latency = 0; // TODO: fill this in based on source type or even see if ffprobe can help.
        }

        // Fill in other per-channel parameters.
        if (channel.uid.empty()) {
            channel.uid = generateUid();
        }
        if (channel.ffmpeg.filterZmq.empty()) {
            channel.ffmpeg.filterZmq = "ipc:///tmp/rise-ffmpeg-zmq_" + sanitizePathToFilename(path + "_" + channel.uid);
        }

        // Fill in other parameters of each quality.
        for (Config::Quality &q: channel.qualities) {
            fillInQuality(q, config, channel);
        }
    }

    /* Fill in the compute trade-off. */
    fillInCompute(config);
}

Awaitable<void> Config::fillInDefaults(IOContext &ioc, Root &config)
{
    // This has ot be a co_await because otherwise the lambda would go out of scope.
    co_await fillInDefaults([&ioc](const std::string &url, const std::vector<std::string> &arguments) ->
                            Awaitable<MediaInfo::SourceInfo> {
        co_return co_await Ffmpeg::ffprobe(ioc, url, arguments);
    }, config);
}
