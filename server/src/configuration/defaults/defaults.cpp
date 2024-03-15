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

void Config::fillInInitialDefaults(Root &config)
{
    /* Set up separated ingests for channels that listen for their source rather than connecting to or otherwise reading
       from their source directly. */
    //  - The ingest:// protocol refers to one of the elements of Config::Root::separatedIngestSources. It implicitly
    //    points at the server, and is the form intended to be used when separeted ingest is configured manually.
    //  - The ingest_http:// protocol is like like http://, except that the endpoint is either stream or probe,
    //    depending on whether it's being streamed from or probed. It has to include the server's address, and the
    //    /ingest directory.
    //  - Sources that listen for connections can't be directly probed (because then the connection would have to be
    //    re-established for ffmpeg), so they're accessed by separated ingest.
    //  - The keys for Config::Root::separatedIngestSources correspond to ingest:// URLs.
    //  - This step converts from the listen flag to the separated ingest form using ingest://.
    //  - All ingest:// URLs (including manually specified ones) are replaced with their corresponding ingest_http://
    //    URLs in fillInDefaults.
    //  - Those URLs are converted to http:// URLs by Ffmpeg::ffprobe and Ffmpeg::Arguments.
    {
        int id = 0;
        for (auto &[path, channel]: config.channels) {
            // Only change channels that listen.
            if (!channel.source.listen) {
                continue;
            }

            // Assign a name to the ingest.
            std::string name = "__listen__/" + std::to_string(id++);

            // Create the ingest from the source. This assumes that the FFMPEG protocol supports the -listen flag.
            SeparatedIngestSource &ingest = config.separatedIngestSources[name];
            ingest = {
                .url = std::move(channel.source.url),
                .arguments = std::move(channel.source.arguments)
            };
            ingest.arguments.push_back("-listen");
            ingest.arguments.push_back("1");

            // Update the channel. The move above clears the arguments.
            channel.source.url = "ingest://" + name; // This is further filled in by fillInDefaults.
            channel.source.listen = false;
        }
    }
}

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
        // Replace ingest:// URLs with ingest_http:// URLs.
        if (channel.source.url.starts_with("ingest://")) {
            channel.source.url = "ingest_http://localhost:" + std::to_string(config.network.port) + "/ingest/" +
                                 channel.source.url.substr(9);
        }

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
