#pragma once

#include "server/CacheKind.hpp"
#include "server/Error.hpp"
#include "server/Resource.hpp"
#include "server/SynchronousResource.hpp"

namespace Server
{

/**
 * A resource that always returns an error.
 *
 * This is useful, for example, to control the caching of the not-found error for segments that have not entered
 * pre-availability but will do so before the normal error would expire from the cache.
 */
class ErrorResource final : public SynchronousResource
{
public:
    ~ErrorResource() override;

    /**
     * @param error The error to return to any request.
     * @param cacheKind The caching to use for the resource.
     * @param isPublic Whether the resource should be available publicly.
     */
    explicit ErrorResource(
        Error error,
        CacheKind cacheKind,
        bool isPublic = false,
        bool allowGet = true,
        bool allowPost = false,
        bool allowPut = false
    ) :
        SynchronousResource(isPublic),
        error(std::move(error)),
        cacheKind(cacheKind),
        allowGet(allowGet),
        allowPost(allowPost),
        allowPut(allowPut)
    {}

    void getSync(Response &response, const Request &request) override;
    void postSync(Response &response, const Request &request) override;
    void putSync(Response &response, const Request &request) override;

private:
    const Error error;
    const CacheKind cacheKind;

    // Verbs marked as true here will respond with the error. Others will get HTTP 405 as usual.
    const bool allowGet;
    const bool allowPost;
    const bool allowPut;

    [[noreturn]] void sendError(Response& response);
};

} // namespace Server
