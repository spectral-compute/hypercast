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
     * Wait for this object to cache the result of ffprobe.
     *
     * This is useful to guarantee that a previously cached probe will remain in scope until this object has a reference
     * to it, rather than allowing this object to do another probe depending on a race condition.
     */
    Awaitable<void> waitForProbe();

    /**
     * Send SIGTERM to the ffmpeg process and wait for it to terminate.
     */
    Awaitable<void> kill();

private:
    Log::Context log;
    Subprocess::Subprocess subprocess;
    Event event;
    bool capturedProbe = false;
    bool finishedReading = false;
};

} // namespace Ffmpeg

/// @}
