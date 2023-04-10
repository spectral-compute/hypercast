#ifndef NDEBUG

#include "FullConfigResource.hpp"

#include "configuration/configuration.hpp"
#include "server/Response.hpp"

Api::FullConfigResource::~FullConfigResource() = default;

void Api::FullConfigResource::getSync(Server::Response &response, const Server::Request &)
{
    response.setCacheKind(Server::CacheKind::none);
    response << config.toJson();
}

#endif // NDEBUG
