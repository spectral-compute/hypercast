#pragma once

#include "util/asio.hpp"

#include <functional>
#include <string>
#include <vector>

namespace MediaInfo
{

struct SourceInfo;

} // namespace MediaInfo

namespace Config
{

class Root;

/**
 * A function that probes a media source with ffprobe and returns information about it.
 *
 * The argument types are to suit Ffmpeg::ProbeCache and Source.
 */
using ProbeFunction = std::function<Awaitable<MediaInfo::SourceInfo>(const std::string &url,
                                                                     const std::vector<std::string> &arguments)>;

/**
 *
 * Fill in defaults for a configuration.
 *
 * @param probe The function to use for probing a media source. The reason for this parameter is so that it is possible
 *              to cache the result of probes that are for devices that are already in use. This also makes it possible
 *              to capture probe results.
 * @param config The configuration to fill in.
 * @return A map from channel in the configuration to the corresponding source information. This only contains entries
 *         for channels that had to be probed.
 */
Awaitable<void> fillInDefaults(const ProbeFunction &probe, Root &config);

/**
 * Fill in defaults for a configuration using ffprobe.
 *
 * @param config The configuration to fill in.
 * @return A map from channel in the configuration to the corresponding source information. This only contains entries
 *         for channels that had to be probed.
 */
Awaitable<void> fillInDefaults(IOContext &ioc, Root &config);

} // namespace Config
