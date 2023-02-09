#include "SynchronousResource.hpp"

#include "Request.hpp"

#include "util/asio.hpp"

Server::SynchronousResource::~SynchronousResource() = default;

Awaitable<void> Server::SynchronousResource::operator()(Response &response, Request &request)
{
    (*this)(response, request, co_await request.readAll());
}

Server::SynchronousStringResource::~SynchronousStringResource() = default;

Awaitable<void> Server::SynchronousStringResource::operator()(Response &response, Request &request)
{
    std::vector<std::byte> requestData = co_await request.readAll();
    (*this)(response, request, std::string_view((const char *)requestData.data(), requestData.size()));
}

Server::SynchronousNullaryResource::~SynchronousNullaryResource() = default;

Awaitable<void> Server::SynchronousNullaryResource::operator()(Response &response, Request &request)
{
    co_await request.readEmpty();
    (*this)(response, const_cast<const Request &>(request));
}
