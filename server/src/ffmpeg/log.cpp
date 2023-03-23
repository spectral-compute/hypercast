#include "log.hpp"

#include <cassert>
#include <optional>

namespace
{

/**
 * Removes spaces from both sides of a string.
 *
 * @param string A string which might have spaces on either or both sides.
 * @return A string with no spaces on either side.
 */
std::string stripSpaces(std::string_view string)
{
    size_t end = string.find_last_not_of(' ');
    string = string.substr(string.find_first_not_of(' '), (end == std::string::npos) ? std::string::npos : end + 1);
    return std::string(string.begin(), string.end());
}

/**
 * Set the parsed ffmpeg log line to just have the entire line as its text and an unknown level string.
 *
 * This is used when parsing failed.
 */
void setToUnknown(Ffmpeg::ParsedFfmpegLogLine &log, std::string_view line)
{
    log.level = Log::Level::error;
    log.levelString.clear();
    log.source.clear();
    log.sourceAddress.clear();
    log.message = line;
}

/**
 * Parse a subtring matching the regex "\[[^]]*\] ".
 *
 * @param remainingLine At entry: the string to extract the square bracket part from. At exit, the substring after the
 *                      brackets and space (not empty on success).
 * @return The contents of the square brackets (not empty), or empty if parsing failed.
 */
std::string_view parseSquareBrackets(std::string_view &remainingLine)
{
    /* The input has to be at least 4 characters long ("[.] ") and start with a [. */
    if (remainingLine.size() < 4 || remainingLine[0] != '[') {
        return {};
    }

    /* Look for the end ] and make sure we found it, the space that follows it, a character after that, and a character
       of content. */
    size_t end = remainingLine.find_first_of(']');
    if (end < 2 || end > remainingLine.size() - 2 || remainingLine[end] != ']' || remainingLine[end + 1] != ' ') {
        return {};
    }

    /* Split the input in two. */
    std::string_view result = remainingLine.substr(1, end - 1);
    remainingLine = remainingLine.substr(end + 2);
    assert(!remainingLine.empty());
    return result;
}

/**
 * Figure out what our log level is from ffmpeg's.
 */
std::optional<Log::Level> getLogLevelFromFfmpegLogLevel(std::string_view ffmpegLogLevel)
{
    for (auto [name, level]: std::initializer_list<std::pair<std::string_view, Log::Level>>{
        { "trace", Log::Level::debug }, { "debug", Log::Level::debug }, { "verbose", Log::Level::debug },
        { "info", Log::Level::info },
        { "warning", Log::Level::warning },
        { "error", Log::Level::error },
        { "fatal", Log::Level::fatal }, { "panic", Log::Level::fatal },
    }) {
        if (ffmpegLogLevel == name) {
            return level;
        }
    }
    return {};
}

} // namespace

Ffmpeg::ParsedFfmpegLogLine::ParsedFfmpegLogLine(std::string_view line)
{
    std::string_view remainingLine = line;

    /* Parse the level string or source. */
    std::string_view sourceOrLevelView = parseSquareBrackets(remainingLine);
    if (sourceOrLevelView.empty()) {
        setToUnknown(*this, line);
        return;
    }

    /* See if there's another [. If so, then what we just got was the subject, and not the level. */
    if (remainingLine[0] == '[') {
        // Read the second entry. This will be the log level.
        std::string_view levelView = parseSquareBrackets(remainingLine);
        if (levelView.empty()) {
            setToUnknown(*this, line);
            return;
        }
        levelString = levelView;

        /* See if sourceOrLevelView can be split into a component/address pair. */
        size_t sourceSplit = sourceOrLevelView.find(" @ ");
        if (sourceSplit == std::string::npos) {
            source = sourceOrLevelView;
        }
        else {
            source = sourceOrLevelView.substr(0, sourceSplit);
            sourceAddress = sourceOrLevelView.substr(sourceSplit + 3);
        }
    }
    else {
        // Otherwise, pos represents the start of the message text.
        levelString = sourceOrLevelView;
    }

    /* Parse the log level. */
    std::optional<Log::Level> maybeLogLevel = getLogLevelFromFfmpegLogLevel(levelString);
    if (!maybeLogLevel) {
        setToUnknown(*this, line);
        return;
    }
    level = *maybeLogLevel;

    /* The remaining string should be the message text. */
    message = stripSpaces(remainingLine);
}

Ffmpeg::ParsedFfmpegLogLine::operator nlohmann::json() const
{
    nlohmann::json j = { { "message", std::move(message) } };
    if (!levelString.empty()) {
        j["level"] = std::move(levelString);
    }
    if (!source.empty()) {
        j["source"] = std::move(source);
    }
    if (!sourceAddress.empty()) {
        j["source_address"] = std::move(sourceAddress);
    }
    return j;
}
