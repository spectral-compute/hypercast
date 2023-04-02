#include "ConfigResource.h"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"

Api::ConfigResource::~ConfigResource() = default;

Api::ConfigResource::ConfigResource(Server::State &state) : Resource(false), serverState(state) {}

Awaitable<void> Api::ConfigResource::getAsync(Server::Response &response, Server::Request &request)
{
    response.setCacheKind(Server::CacheKind::none);

    if (!(co_await request.readSome()).empty()) {
        throw Server::Error(Server::ErrorKind::BadRequest);
    }

    response << serverState.requestedConfig.jsonRepresentation;
    co_return;
}

Awaitable<void> Api::ConfigResource::putAsync(Server::Response &response, Server::Request &request)
{
    co_await serverState.applyConfiguration(Config::Root::fromJson(co_await request.readAllAsString()));
}

size_t Api::ConfigResource::getMaxPutRequestLength() const noexcept
{
    return 1 << 18; // 256 kiB.
}
