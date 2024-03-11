#include "Process.hpp"

#include "Arguments.hpp"
#include "log.hpp"

#include "util/asio.hpp"
#include "util/json.hpp"
#include "util/util.hpp"

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
        result += "\" ";
    }
    result.resize(result.size() - 1); // Remove the trailing space.
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

    // Interpret ffmpeg's log-level system.
    Ffmpeg::ParsedFfmpegLogLine parsedLine = line;

    /* Write to the log. */
    log << "stderr" << parsedLine.level << parsedLine.message;
}

/**
 * Handle a line from ffmpeg's stderr.
 */
void handleFfmpegStdoutLine(Ffmpeg::Timestamp &pts, std::string_view line)
{
    /* Extract the values from the line. */
    std::string_view valueStr;
    std::string_view timeBaseStr;
    std::string_view timeBaseNumeratorStr;
    std::string_view timeBaseNumeratorDenominatorStr;

    Util::split(line, { valueStr, timeBaseStr });
    Util::split(timeBaseStr, { timeBaseNumeratorStr, timeBaseNumeratorDenominatorStr }, '/');

    /* Convert to integers. */
    int64_t value = Util::parseInt64(valueStr);
    int64_t timeBaseNumerator = Util::parseInt64(timeBaseNumeratorStr);
    int64_t timeBaseNumeratorDenominator = Util::parseInt64(timeBaseNumeratorDenominatorStr);

    /* Check the timebase is valid. */
    if (timeBaseNumerator == 0 || timeBaseNumeratorDenominator == 0) {
        throw std::runtime_error("Timestamp with non-finite time base.");
    }

    /* Update the timestamp if the new one is actually newer. */
    Ffmpeg::Timestamp newPts(value, timeBaseNumerator, timeBaseNumeratorDenominator);
    if (newPts > pts) {
        pts = newPts;
    }
}

} // namespace

Ffmpeg::Process::Process(IOContext &ioc, Log::Log &log, Arguments arguments) :
    log(log("ffmpeg")), subprocess(ioc, "ffmpeg", arguments.getFfmpegArguments(), false), event(ioc)
{
    /* Log the arguments given to ffmpeg. */
    this->log << "arguments" << Log::Level::info << getArgumentsForLog(arguments.getFfmpegArguments());

    /* Create a coroutine to handle the stderr output of ffmpeg and wait for the process termination. */
    spawnDetached(ioc, [this, &ioc, arguments = std::move(arguments)]() -> Awaitable<void> {
        // Make sure that while ffmpeg is running, ffprobe returns a cached result.
        ProbeResult probeResult =
            co_await ffprobe(ioc, arguments.getSourceUrl(), std::span(arguments.getSourceArguments()));
        capturedProbe = true;
        event.notifyAll();

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
        finishedReadingStderrAndTerminated = true;
        event.notifyAll();
    });

    /* Create another coroutine to handle the stdout output of ffmpeg. */
    spawnDetached(ioc, [this]() -> Awaitable<void> {
        // Read the timestamps that ffmpeg emits.
        try {
            while (auto line = co_await this->subprocess.readStdoutLine()) {
                bool isFirst = !pts;
                handleFfmpegStdoutLine(pts, *line);
                if (isFirst) {
                    event.notifyAll();
                }
            }
        }
        catch (const std::exception &e) {
            this->log << "exception" << Log::Level::error << e.what();
        }
        catch (...) {
            this->log << "exception" << Log::Level::error << "Unknown.";
        }

        // Wait for ffmpeg to terminate, and then notify anything that's waiting that we've done so.
        finishedReadingStdout = true;
        event.notifyAll();
    });
}

Awaitable<void> Ffmpeg::Process::waitForProbe()
{
    while (!capturedProbe) {
        co_await event.wait();
    }
}

Awaitable<void> Ffmpeg::Process::kill()
{
    subprocess.kill();
    while (!finishedReadingStderrAndTerminated || !finishedReadingStdout) {
        co_await event.wait();
    }
}

Awaitable<Ffmpeg::Timestamp> Ffmpeg::Process::getPts() const
{
    /* Wait for the PTS. */
    while (!pts && !finishedReadingStdout) {
        co_await event.wait();
    }

    /* It's possible ffmpeg will terminate before emitting a PTS. */
    if (!pts) {
        throw std::runtime_error("No PTS ever emitted to ffmpeg stdout.");
    }

    /* We have a PTS. */
    co_return pts;
}
