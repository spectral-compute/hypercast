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
     * The log level we should use in our logging system.
     */
    Log::Level level = Log::Level::fatal;

    /**
     * The actual text of the message.
     */
    std::string message;
};

} // namespace Ffmpeg
