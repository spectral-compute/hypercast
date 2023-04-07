#pragma once

#include "ffprobe.hpp"

#include "log/Log.hpp"
#include "util/Event.hpp"
#include "util/subprocess.hpp"

/**
 * @defgroup ffmpeg FFmpeg
 *
 * Contains stuff for interacting with FFmpeg.
 */
/// @addtogroup ffmpeg
/// @{

/**
 * Contains stuff for interacting with FFmpeg.
 */
namespace Ffmpeg
{

class Arguments;

/**
 * Wraps an ffmpeg process to provide logging.
 */
class Process final
{
public:
    /**
     * Create an ffmpeg subprocess, and log its output.
     *
     * @param arguments The arguments to give to ffmpeg.
     */
    explicit Process(IOContext &ioc, Log::Log &log, Arguments arguments);

    /**
     * Send SIGTERM to the ffmpeg process and wait for it to terminate.
     */
    Awaitable<void> kill();

private:
    Log::Context log;
    Subprocess::Subprocess subprocess;
    Event finishedReadingEvent;
    bool finishedReading = false;
};

} // namespace Ffmpeg

/// @}
