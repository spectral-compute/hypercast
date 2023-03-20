#include "ErrorResource.hpp"

#include "server/Response.hpp"

#include "util/asio.hpp"

Server::ErrorResource::~ErrorResource() = default;

Awaitable<void> Server::ErrorResource::operator()(Response &response, Request &)
{
    response.setCacheKind(cacheKind);
    throw Error(error);
}

bool Server::ErrorResource::getAllowGet() const noexcept
{
    return allowGet;
}

bool Server::ErrorResource::getAllowPost() const noexcept
{
    return allowPost;
}

bool Server::ErrorResource::getAllowPut() const noexcept
{
    return allowPut;
}
