#include "PutResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/debug.hpp"

Server::PutResource::~PutResource() = default;

void Server::PutResource::operator()(Response &response, const Request &request, std::vector<std::byte> requestData)
{
    switch (request.getType()) {
        case Request::Type::get:
            response.setCacheKind(cacheKind);
            if (!requestData.empty()) {
                throw Error(ErrorKind::BadRequest);
            }
            if (!hasBeenPut) {
                throw Error(ErrorKind::NotFound);
            }
            response << data;
            return;
        case Request::Type::put:
            assert(!request.getIsPublic());
            response.setCacheKind(CacheKind::none);
            data = std::move(requestData);
            hasBeenPut = true;
            return;
        default: unreachable();
    }
}

bool Server::PutResource::getAllowGet() const noexcept
{
    return true;
}

bool Server::PutResource::getAllowPut() const noexcept
{
    return true;
}
