#pragma once

#include "log/Level.hpp"

#include "nlohmann/json.hpp"

#include <string>
#include <string_view>

namespace Ffmpeg
{

/**
 * A parsed line of logging output from ffmpeg.
 */
struct ParsedFfmpegLogLine final
{
    /**
     * Parse a line of logging output from ffmpeg.
     */
    ParsedFfmpegLogLine(std::string_view line);

    /**
     * Convert the relevant fields of this object to a JSON object.
     */
    operator nlohmann::json() const;

    /**
     * The log level we should use in our logging system.
     */
    Log::Level level = Log::Level::fatal;

    /**
     * The log level ffmpeg used.
     *
     * Ffmpeg has a super set of log levels to us.
     */
    std::string levelString;

    /**
     * Sometimes ffmpeg outputs the source of a message.
     */
    std::string source;

    /**
     * Ffmpeg usually outputs an address along with a source.
     */
    std::string sourceAddress;

    /**
     * The actual text of the message.
     */
    std::string message;
};

} // namespace Ffmpeg
