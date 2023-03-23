#pragma once

#include "log/Log.hpp"
#include "util/subprocess.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Config
{

class Root;

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

private:
    Log::Context log;
    Subprocess::Subprocess subprocess;
};

/**
 * Generate the arguments to ffmpeg.
 *
 * These arguments should be given to FfmpegProcess.
 *
 * @param config The configuration object.
 * @param basePath The base path for the DASH streams.
 * @return The arguments to give to FfmpegProcess.
 */
std::vector<std::string> getFfmpegArguments(const Config::Root &config, std::string_view basePath);

} // namespace Ffmpeg

/// @}
