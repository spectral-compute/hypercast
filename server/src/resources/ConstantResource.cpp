#include "ConstantResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"

#include <cassert>

Server::ConstantResource::~ConstantResource() = default;

void Server::ConstantResource::operator()(Response &response, [[maybe_unused]] const Request &request)
{
    assert(request.getPath().empty());
    assert(request.getType() == Request::Type::get);

    response.setCacheKind(cacheKind);
    response.setMimeType(mimeType);
    response << content;
}

bool Server::ConstantResource::getAllowGet() const noexcept
{
    return true;
}
