#pragma once

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
