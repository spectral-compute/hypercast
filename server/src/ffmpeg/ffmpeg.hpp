#pragma once

#include "log/Log.hpp"
#include "util/Event.hpp"
#include "util/subprocess.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Config
{

struct Channel;
struct Network;

} // namespace Config

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

/**
 * Wraps an ffmpeg process to provide logging.
 */
class FfmpegProcess final
{
public:
    /**
     * Create an ffmpeg subprocess, and log its output.
     *
     * @param arguments The arguments to give to ffmpeg.
     */
    explicit FfmpegProcess(IOContext &ioc, Log::Log &log, std::span<const std::string> arguments);

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

/**
 * Generate the arguments to ffmpeg.
 *
 * These arguments should be given to FfmpegProcess.
 *
 * @param channelConfig The configuration object for the specific channel.
 * @param networkConfig The channel configuration object for the network.
 * @param uidPath The base path for the DASH streams.
 * @return The arguments to give to FfmpegProcess.
 */
std::vector<std::string> getFfmpegArguments(const Config::Channel &channelConfig, const Config::Network &networkConfig,
                                            std::string_view uidPath);

} // namespace Ffmpeg

/// @}
