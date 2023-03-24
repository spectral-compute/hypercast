#include "Resource.hpp"
#include "Error.hpp"
#include "util/debug.hpp"
#include <util/asio.hpp>
#include "server/Request.hpp"
#include "server/Response.hpp"

Server::Resource::~Resource() = default;

bool Server::Resource::getAllowNonEmptyPath() const noexcept { return false; }

Awaitable<void> Server::Resource::getAsync(Response&, Request&) {
    unsupportedHttpVerb("GET");
}
Awaitable<void> Server::Resource::postAsync(Response&, Request&) {
    unsupportedHttpVerb("POST");
}
Awaitable<void> Server::Resource::putAsync(Response&, Request&) {
    unsupportedHttpVerb("PUT");
}

Awaitable<void> Server::Resource::operator()(Response &response, Request &request) {
    switch (request.getType()) {
        case Server::Request::Type::get:
            co_await this->getAsync(response, request);
            break;
        case Server::Request::Type::post:
            co_await this->postAsync(response, request);
            break;
        case Server::Request::Type::put:
            co_await this->putAsync(response, request);
            break;
    }
}

void Server::Resource::unsupportedHttpVerb(const std::string& verb) const {
    throw Error(ErrorKind::UnsupportedType, verb + " is not supported by this resource");
}
