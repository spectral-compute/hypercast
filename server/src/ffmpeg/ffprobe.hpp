#pragma once

#include "media/MediaInfo.hpp"
#include "util/asio.hpp"

#include <span>
#include <string>
#include <string_view>

namespace Ffmpeg
{

/**
 * Get information about a media source via ffprobe.
 *
 * @param url The URL of the source to get media information about. This is the part that appears after -i in ffmpeg.
 * @param arguments The arguments to give to ffprobe (and would be given to ffmpeg) before the URL.
 * @return Information about the media source.
 */
Awaitable<MediaInfo::SourceInfo> ffprobe(IOContext &ioc, std::string_view url,
                                         std::span<const std::string_view> arguments = {});

/**
 * @copydoc ffprobe
 */
Awaitable<MediaInfo::SourceInfo> ffprobe(IOContext &ioc, std::string_view url,
                                         std::span<const std::string> arguments);

} // namespace Ffmpeg
