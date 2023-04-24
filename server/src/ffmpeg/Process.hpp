#pragma once

#include "ffprobe.hpp"
#include "Timestamp.hpp"

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

    /**
     * Get the presentation timestamp of the output.
     *
     * In principle, this could be different for different streams, but they should be synchronized, and thus one is
     * chosen as the representative.
     *
     * This only works if the arguments given to the process include the arguments to output these timestamps in the
     * format this class expects. Arguments::liveStream does this.
     *
     * This method waits until the first timestamp arrives. Otherwise, it returns the latest timestamp.
     */
    Awaitable<Timestamp> getPts() const;

private:
    Log::Context log;
    Subprocess::Subprocess subprocess;
    Event event;
    bool capturedProbe = false;
    bool finishedReadingStdout = false;
    bool finishedReadingStderrAndTerminated = false;
    Timestamp pts;
};

} // namespace Ffmpeg

/// @}
