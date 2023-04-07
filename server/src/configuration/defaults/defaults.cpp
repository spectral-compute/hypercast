#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"
#include "ffmpeg/ffprobe.hpp"

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
            channel.qualities.emplace_back(Quality{});
        }

        // Fill in the information we get from ffprobe. This is done first because a lot of other stuff is based on
        // this.
        co_await fillInQualitiesFromFfprobe(channel.qualities, channel.source, probe);

        // Fill in prerequisites to the latency tracker.
        if (!channel.source.latency) {
            channel.source.latency = 0; // TODO: fill this in based on source type or even see if ffprobe can help.
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
