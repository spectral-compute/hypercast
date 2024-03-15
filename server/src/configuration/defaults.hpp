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
 * Fill in initial defaults for a configuration.
 *
 * The initial defaults are those defaults that have to be filled in so that some initial stuff (like ingest ffmpeg
 * processes) can be sorted out that the main fillInDefaults function uses.
 *
 * This function:
 *  - Configures separated ingest for channels for which channels.source.listen field is true.
 *
 * If it's known that your configuration does not do any of the above, then it's not strictly necessary to call this
 * method.
 *
 * @param config The configuration to fill in.
 */
void fillInInitialDefaults(Root &config);

/**
 *
 * Fill in defaults for a configuration.
 *
 * @param probe The function to use for probing a media source. The reason for this parameter is so that it is possible
 *              to cache the result of probes that are for devices that are already in use. This also makes it possible
 *              to capture probe results.
 * @param config The configuration to fill in.
 */
Awaitable<void> fillInDefaults(const ProbeFunction &probe, Root &config);

} // namespace Config
