#include "ffmpeg.hpp"

#include "log.hpp"

#include "util/asio.hpp"
#include "util/json.hpp"

namespace
{

/**
 * Handle a line from ffmpeg's stderr.
 */
void handleFfmpegStderrLine(Log::Context &log, std::string_view line)
{
    /* Ignore empty lines. */
    if (line.empty()) {
        return;
    }

    /* Parse the line. */
    Ffmpeg::ParsedFfmpegLogLine parsedLine = line;
    nlohmann::json j = parsedLine;

    /* Write to the log. */
    log << "stderr" << parsedLine.level << Json::dump(j);
}

} // namespace

Ffmpeg::FfmpegProcess::FfmpegProcess(IOContext &ioc, Log::Log &log, std::span<const std::string> arguments) :
    log(log("ffmpeg")), subprocess(ioc, "ffmpeg", arguments, false, false)
{
    /* Create a coroutine to handle the stderr output of ffmpeg and wait for the process termination. */
    spawnDetached(ioc, this->log, [this]() -> Awaitable<void> {
        while (auto line = co_await this->subprocess.readStderrLine()) {
            handleFfmpegStderrLine(this->log, *line);
        }
        co_await this->subprocess.wait();
    });
}
