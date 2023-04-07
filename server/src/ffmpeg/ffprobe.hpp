#pragma once

#include "media/MediaInfo.hpp"
#include "util/asio.hpp"

#include <span>
#include <string>
#include <string_view>

namespace Ffmpeg
{

/**
 * A caching result of running ffprobe.
 *
 * Instances of this class hold the result of running ffprobe. It's like a shared pointer, and the result is cached
 * until all copies of it are deleted. It's not valid to probe the same URL with different parameters while a result is
 * still cached for that URL.
 */
class ProbeResult final
{
public:
    ~ProbeResult();

    ProbeResult(const ProbeResult &) = default;
    ProbeResult(ProbeResult &&) = default;
    ProbeResult &operator=(const ProbeResult &);
    ProbeResult &operator=(ProbeResult &&);

    /**
     * Get the actual media source information.
     *
     * This throws exceptions that occurred while running ffmpeg.
     */
    operator const MediaInfo::SourceInfo &() const;

    /**
     * Get the actual media source information.
     *
     * This throws exceptions that occurred while running ffmpeg.
     */
    const MediaInfo::SourceInfo &operator*() const
    {
        return *this;
    }

private:
    friend Awaitable<Ffmpeg::ProbeResult> ffprobe(IOContext &ioc, std::string_view url,
                                                  std::vector<std::string> arguments);
    struct CacheEntry;
    explicit ProbeResult(std::shared_ptr<CacheEntry> cacheEntry) : cacheEntry(std::move(cacheEntry)) {}
    std::shared_ptr<CacheEntry> cacheEntry;
};

/**
 * Get information about a media source via ffprobe.
 *
 * @param url The URL of the source to get media information about. This is the part that appears after -i in ffmpeg.
 * @param arguments The arguments to give to ffprobe (and would be given to ffmpeg) before the URL.
 * @return Information about the media source. This result is like a shared pointer, and is cached until all copies of
 *         it are deleted.
 * @throws InUseException
 */
Awaitable<ProbeResult> ffprobe(IOContext &ioc, std::string_view url, std::vector<std::string> arguments);

/**
 * @copydoc ffprobe
 */
Awaitable<ProbeResult> ffprobe(IOContext &ioc, std::string_view url, std::span<const std::string> arguments);

/**
 * @copydoc ffprobe
 */
Awaitable<ProbeResult> ffprobe(IOContext &ioc, std::string_view url, std::span<const std::string_view> arguments = {});

} // namespace Ffmpeg
