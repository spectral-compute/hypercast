#pragma once

#include "media/MediaInfo.hpp"
#include "util/asio.hpp"

namespace Config
{

struct Source;

} // namespace Config

namespace Ffmpeg
{

/**
 * Get information about a media source via ffprobe.
 *
 * @param source The source to get media information about.
 * @return Information about the media source.
 */
Awaitable<MediaInfo::SourceInfo> ffprobe(IOContext &ioc, const Config::Source &source);

} // namespace Ffmpeg
