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
     */
    explicit PutResource(CacheKind cacheKind = CacheKind::fixed, bool isPublic = false) :
        SynchronousResource(isPublic), cacheKind(cacheKind)
    {
    }

    void getSync(Response &response, const Request &request) override;
    void putSync(Response &response, const Request &request) override;

private:
    const CacheKind cacheKind;
    std::vector<std::byte> data;
    bool hasBeenPut = false; ///< Whether anything has been put.
};

} // namespace Server
