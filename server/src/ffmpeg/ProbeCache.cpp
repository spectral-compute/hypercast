#include "ProbeCache.hpp"

#include "util/asio.hpp"

// These are expensive.
Ffmpeg::ProbeCache::~ProbeCache() = default;
Ffmpeg::ProbeCache::ProbeCache() = default;
Ffmpeg::ProbeCache &Ffmpeg::ProbeCache::operator=(ProbeCache &&) = default;

bool Ffmpeg::ProbeCache::contains(const std::string &url) const noexcept
{
    return cache.contains(url);
}

const MediaInfo::SourceInfo *Ffmpeg::ProbeCache::operator[](const std::string &url,
                                                            const std::vector<std::string> &arguments) const noexcept
{
    /* See if the URL exists in the cache at all. */
    auto it1 = cache.find(url);
    if (it1 == cache.end()) {
        return nullptr;
    }

    /* If it does, see if the arguments exist for that URL, and if so, return the corresponding source info. */
    auto it2 = it1->second.find(arguments);
    return (it2 == it1->second.end()) ? nullptr : &it2->second;
}

void Ffmpeg::ProbeCache::insert(MediaInfo::SourceInfo sourceInfo, const std::string &url,
                                const std::vector<std::string> &arguments)
{
    cache[url][arguments] = sourceInfo;
}
