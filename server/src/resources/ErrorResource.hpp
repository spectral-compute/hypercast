#pragma once

#include "server/CacheKind.hpp"
#include "server/Error.hpp"
#include "server/Resource.hpp"

namespace Server
{

/**
 * A resource that always returns an error.
 *
 * This is useful, for example, to control the caching of the not-found error for segments that have not entered
 * pre-availability but will do so before the normal error would expire from the cache.
 */
class ErrorResource final : public Resource
{
public:
    ~ErrorResource() override;

    /**
     * Construct a resource with constant content.
     *
     * @param error The error to return to any request.
     * @param cacheKind The caching to use for the resource.
     * @param isPublic Whether the resource should be available publicly.
     */
    ErrorResource(Error error, CacheKind cacheKind, bool isPublic = false, bool allowGet = true, bool allowPost = false,
                  bool allowPut = false) :
        Resource(isPublic), error(std::move(error)), cacheKind(cacheKind), allowGet(allowGet), allowPost(allowPost),
        allowPut(allowPut)
    {
    }

    Awaitable<void> operator()(Response &response, Request &request) override;

    bool getAllowGet() const noexcept override;
    bool getAllowPost() const noexcept override;
    bool getAllowPut() const noexcept override;

private:
    const Error error;
    const CacheKind cacheKind;
    const bool allowGet;
    const bool allowPost;
    const bool allowPut;
};

} // namespace Server
