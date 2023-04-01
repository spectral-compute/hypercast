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
void Server::Resource::defaultOptionsHandler(Response& res, const Request&) {
    // This default implementation is plausibly all you ever need, but it can of course be overridden.
    // Mostly, this exists to make CORS preflight work. It may very well be buggy if used for other purposes.
    // Practically-speaking, though, CORS preflight is the only thing OPTIONS is really used for. In most cases
    // it's simpler/faster to just *do the request* and see what happens.
    // Note that this will not set status 204. 200 is acceptable for an OPTIONS response.

    // Note that we already checked for forbidden and not-found cases before this function is called.

    // TODO: This should read the request headers to determine if it's a CORS preflight request and send the
    // list of allowed verbs in either one of the two output headers. But the `Request` class is stupid (why
    // is half of the needed stuff hiding in HttpRequest?!), so we cannot access request headers here for some
    // fucking reason.

    res.setHeader(boost::beast::http::field::allow, "OPTIONS, GET, POST, PUT, HEAD");
    res.setHeader(boost::beast::http::field::access_control_allow_methods, "OPTIONS, GET, POST, PUT, HEAD");
}

Awaitable<void> Server::Resource::optionsAsync(Response& res, Request& req) {
    defaultOptionsHandler(res, req);
    co_return;
}

Awaitable<void> Server::Resource::operator()(Response &response, Request &request) {
    switch (request.getType()) {
        case Server::Request::Type::get: return getAsync(response, request);
        case Server::Request::Type::post: return postAsync(response, request);
        case Server::Request::Type::put: return putAsync(response, request);
        case Server::Request::Type::options: return optionsAsync(response, request);
    }
    unreachable();
}

size_t Server::Resource::getMaxGetRequestLength() const noexcept
{
    return 0;
}

size_t Server::Resource::getMaxPostRequestLength() const noexcept
{
    return 0;
}

size_t Server::Resource::getMaxPutRequestLength() const noexcept
{
    return 0;
}

void Server::Resource::unsupportedHttpVerb(const std::string& verb) const {
    throw Error(ErrorKind::UnsupportedType, verb + " is not supported by this resource");
}
