#pragma once

#include "server/CacheKind.hpp"
#include "server/SynchronousResource.hpp"

namespace Server
{

/**
 * A resource that can be written to privately with PUT and potentially read from publicly with GET.
 *
 * This is useful because ffmpeg writes some resources, such as the manifest.mpd file.
 */
class PutResource final : public SynchronousResource
{
public:
    ~PutResource() override;

    /**
     * Construct a resource with constant content.
     *
     * @param cacheKind The caching to use for the resource when GET is used.
     * @param maxRequestLength The maximum length of resource that can be PUT to this resource (i.e: the value to be
     *                         returned by getMaxRequestLength).
     */
    explicit PutResource(CacheKind cacheKind = CacheKind::fixed, size_t maxRequestLength = 1 << 20,
                         bool isPublic = false) :
        SynchronousResource(isPublic), maxRequestLength(maxRequestLength), cacheKind(cacheKind)
    {
    }

    void getSync(Response &response, const Request &request) override;
    void putSync(Response &response, const Request &request) override;

    size_t getMaxRequestLength() const noexcept override;

private:
    const size_t maxRequestLength;
    const CacheKind cacheKind;
    std::vector<std::byte> data;
    bool hasBeenPut = false; ///< Whether anything has been put.
};

} // namespace Server
