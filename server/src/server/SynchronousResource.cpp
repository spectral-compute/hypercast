#include "SynchronousResource.hpp"

#include "Request.hpp"

#include "util/asio.hpp"

Server::SynchronousResource::~SynchronousResource() = default;

Awaitable<std::vector<std::byte>> Server::SynchronousResource::extractData(Request& request) {
    return request.readAll();
}

Awaitable<void> Server::SynchronousResource::operator()(Response &response, Request &request) {
    requestData = co_await extractData(request);
    switch (request.getType()) {
        case Server::Request::Type::get:
            this->getSync(response, request);
            break;
        case Server::Request::Type::post:
            this->postSync(response, request);
            break;
        case Server::Request::Type::put:
            this->putSync(response, request);
            break;
    }
}

void Server::SynchronousResource::getSync(Response&, const Request&) { unsupportedHttpVerb("GET"); }
void Server::SynchronousResource::postSync(Response&, const Request&) { unsupportedHttpVerb("POST"); }
void Server::SynchronousResource::putSync(Response&, const Request&) { unsupportedHttpVerb("PUT"); }


Server::SynchronousNullaryResource::~SynchronousNullaryResource() = default;

Awaitable<std::vector<std::byte>> Server::SynchronousNullaryResource::extractData(Request& request) {
    co_return std::vector<std::byte>{};
}
