#pragma once

#include "media/MediaInfo.hpp"

#include <map>
#include <string>
#include <vector>

template <typename> class Awaitable;
class IOContext;

namespace Ffmpeg
{

/**
 * A cache for the results of running ffprobe.
 *
 * This is useful both to avoid unnecessary calls to ffprobe, and to keep track of the state we need to avoid calling
 * ffprobe on a source that's in use (which is not supported by all sources).
 */
class ProbeCache final
{
public:
    ~ProbeCache();
    explicit ProbeCache();

    // It's not that we can't in principle copy, we just shouldn't because then we'd have multiple caches of something
    // that's really external to the program.
    ProbeCache(const ProbeCache &) = delete;
    ProbeCache(ProbeCache &&) = default;
    ProbeCache &operator=(const ProbeCache &) = delete;
    ProbeCache &operator=(ProbeCache &&);

    /**
     * Look up an entry in the cache.
     *
     * @param url The URL of the entry to look up.
     * @param arguments The arguments for the entry to look up.
     * @return The source information if it exists in the cache, or nullptr otherwise.
     */
    const MediaInfo::SourceInfo *operator[](const std::string &url,
                                            const std::vector<std::string> &arguments) const noexcept;

    /**
     * Tell if a URL exists in the cache.
     *
     * @param url The URL to look up.
     * @return True if any element in the cache has this URL, and false otherwise.
     */
    bool contains(const std::string &url) const noexcept;

    /**
     * Insert a result into the cache.
     *
     * @param sourceInfo The source info result to insert into the cache.
     * @param url The URL for the media source the result corresponds to.
     * @param arguments The ffprobe arguments for the media source the result corresponds to.
     */
    void insert(MediaInfo::SourceInfo sourceInfo, const std::string &url, const std::vector<std::string> &arguments);

private:
    /**
     * A map from URL to map from arguments to result.
     *
     * It's expected that the outer map does not contain empty maps, so if we ever add erasure, empty outer maps will
     * need to be pruned.
     */
    std::map<std::string, std::map<std::vector<std::string>, MediaInfo::SourceInfo>> cache;
};

} // namespace Ffmpeg
