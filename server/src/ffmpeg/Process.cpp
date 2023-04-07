#include "Process.hpp"

#include "Arguments.hpp"
#include "log.hpp"

#include "util/asio.hpp"
#include "util/json.hpp"

namespace
{

/**
 * Escape a character so it's unambiguous when displayed in a string.
 */
const char *escapeChar(char c)
{
    switch (c) {
        case '\\': return "\\\\";
        case '"': return "\\\"";
    }
    return nullptr;
}

/**
 * Format an array of arguments for display as a single string.
 */
std::string getArgumentsForLog(std::span<const std::string> arguments)
{
    std::string result;
    for (const std::string &argument: arguments) {
        result += '"';
        for (char c: argument) {
            if (const char *s = escapeChar(c)) {
                result += s;
            }
            else {
                result += c;
            }
        }
        result += "\", ";
    }
    result.resize(result.size() - 2); // Remove the trailing ", ".
    return result;
}

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

Ffmpeg::Process::Process(IOContext &ioc, Log::Log &log, Arguments arguments) :
    log(log("ffmpeg")), subprocess(ioc, "ffmpeg", arguments.getFfmpegArguments(), false, false),
    finishedReadingEvent(ioc)
{
    /* Log the arguments given to ffmpeg. */
    this->log << "arguments" << Log::Level::info << getArgumentsForLog(arguments.getFfmpegArguments());

    /* Create a coroutine to handle the stderr output of ffmpeg and wait for the process termination. */
    spawnDetached(ioc, [this]() -> Awaitable<void> {
        // Read the logging that ffmpeg emits.
        try {
            while (auto line = co_await this->subprocess.readStderrLine()) {
                handleFfmpegStderrLine(this->log, *line);
            }
        }
        catch (const std::exception &e) {
            this->log << "exception" << Log::Level::error << e.what();
        }
        catch (...) {
            this->log << "exception" << Log::Level::error << "Unknown.";
        }

        // Wait for ffmpeg to terminate, and then notify anything that's waiting that we've done so.
        co_await this->subprocess.wait(false);
        finishedReading = true;
        finishedReadingEvent.notifyAll();
    });
}

Awaitable<void> Ffmpeg::Process::kill()
{
    subprocess.kill();
    while (!finishedReading) {
        co_await finishedReadingEvent.wait();
    }
}
