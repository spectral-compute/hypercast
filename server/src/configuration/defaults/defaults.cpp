#include "configuration/defaults.hpp"
#include "configuration/configuration.hpp"

namespace Config
{

Awaitable<void> fillInQualitiesFromFfprobe(IOContext &ioc, std::vector<Quality> &qualities,  const Source &source);
void fillInQuality(Config::Quality &q, const Root &config);

} // namespace Config

Awaitable<void> Config::fillInDefaults(IOContext &ioc, Root &config)
{
    // TODO: This is likely no longer correct, since the ffprobeage will need to be changed to cope with
    //       multiple input ports.
    /* Fill in some simple defaults. */
    if (!config.log.print) {
        config.log.print = config.log.path.empty(); // By default, print if and only if we're not logging to a file.
    }

    /* If there are no qualities, add one for fillInQualitiesFromFfprobe to fill in. */
    if (config.qualities.empty()) {
        config.qualities.emplace_back(Quality{});
    }

    /* Fill in the information we get from ffprobe. This is done first because a lot of other stuff is based on this. */
    co_await fillInQualitiesFromFfprobe(ioc, config.qualities, config.source);

    /* Fill in prerequisites to the latency tracker. */
    if (!config.source.latency) {
        config.source.latency = 0; // TODO: fill this in based on source type or even see if ffprobe can help.
    }

    /* Fill in other parameters of each quality. */
    for (Config::Quality &q: config.qualities) {
        fillInQuality(q, config);
    }
}
