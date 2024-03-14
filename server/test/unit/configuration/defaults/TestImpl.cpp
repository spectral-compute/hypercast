#include "TestImpl.hpp"

#include "configuration/defaults.hpp"
#include "ffmpeg/ffprobe.hpp"

Awaitable<void> fillInDefaults(IOContext &ioc, Config::Root &config)
{
    // This has ot be a co_await because otherwise the lambda would go out of scope.
    co_await Config::fillInDefaults([&ioc](const std::string &url, const std::vector<std::string> &arguments) ->
                                    Awaitable<MediaInfo::SourceInfo> {
        co_return co_await Ffmpeg::ffprobe(ioc, url, arguments);
    }, config);
}
