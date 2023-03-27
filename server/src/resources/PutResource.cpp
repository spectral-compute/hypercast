#include "PutResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/debug.hpp"

Server::PutResource::~PutResource() = default;

void Server::PutResource::getSync(Response &response, const Request&)
{
    response.setCacheKind(cacheKind);
    if (!requestData.empty()) {
        throw Error(ErrorKind::BadRequest, "Unexpected request data");
    }
    if (!hasBeenPut) {
        throw Error(ErrorKind::NotFound, "PUT resource was GET'd before being PUT");
    }
    response << data;
}

void Server::PutResource::putSync(Response &response, const Request &request)
{
    assert(!request.getIsPublic());
    response.setCacheKind(CacheKind::none);
    data = std::move(requestData);
    hasBeenPut = true;
}

size_t Server::PutResource::getMaxRequestLength() const noexcept
{
    return maxRequestLength;
}
