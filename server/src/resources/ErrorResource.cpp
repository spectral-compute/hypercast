#include "ErrorResource.hpp"

#include "server/Response.hpp"

#include "util/asio.hpp"

Server::ErrorResource::~ErrorResource() = default;

void Server::ErrorResource::sendError(Response &response)
{
    response.setCacheKind(cacheKind);
    throw Error(error);
}


void Server::ErrorResource::getSync(Response& response, const Request&) {
    if (!allowGet) {
        unsupportedHttpVerb("GET");
    }
    sendError(response);
}
void Server::ErrorResource::postSync(Response& response, const Request&) {
    if (!allowPost) {
        unsupportedHttpVerb("POST");
    }
    sendError(response);
}
void Server::ErrorResource::putSync(Response& response, const Request&) {
    if (!allowPut) {
        unsupportedHttpVerb("PUT");
    }
    sendError(response);
}
