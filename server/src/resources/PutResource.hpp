#pragma once

#include "server/CacheKind.hpp"
#include "server/Resource.hpp"

#include <filesystem>
#include <vector>

namespace Server
{

/**
 * A resource that can be written to privately with PUT and potentially read from publicly with GET.
 *
 * This is useful because ffmpeg writes some resources, such as the manifest.mpd file.
 */
class PutResource final : public Resource
{
public:
    ~PutResource() override;

    /**
     * Construct a resource with constant content.
     *
     * @param path The path of the file to write the received data to. If empty, no file is written to.
     * @param cacheKind The caching to use for the resource when GET is used.
     * @param maxRequestLength The maximum length of resource that can be PUT to this resource (i.e: the value to be
     *                         returned by getMaxRequestLength).
     */
    explicit PutResource(IOContext &ioc, std::filesystem::path path, CacheKind cacheKind = CacheKind::fixed,
                         size_t maxRequestLength = 1 << 20, bool isPublic = false) :
        Resource(isPublic), ioc(&ioc), maxRequestLength(maxRequestLength), cacheKind(cacheKind), path(std::move(path))
    {
    }
    explicit PutResource(CacheKind cacheKind = CacheKind::fixed, size_t maxRequestLength = 1 << 20,
                         bool isPublic = false) :
        Resource(isPublic), maxRequestLength(maxRequestLength), cacheKind(cacheKind)
    {
    }

    Awaitable<void> getAsync(Response &response, Request &request) override;
    Awaitable<void> putAsync(Response &response, Request &request) override;

    size_t getMaxRequestLength() const noexcept override;

private:
    IOContext *const ioc = nullptr; ///< The IOContext if we're to save files.
    const size_t maxRequestLength;
    const CacheKind cacheKind;
    const std::filesystem::path path; ///< The path of the file to write to if any.

    std::vector<std::byte> data;
    bool hasBeenPut = false; ///< Whether anything has been put.
};

} // namespace Server
