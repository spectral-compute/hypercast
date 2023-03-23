#include "ConfigResource.h"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/debug.hpp"
#include "util/asio.hpp"

Server::ConfigAPIResource::ConfigAPIResource(State& state):
        Resource(false),
        serverState(state) {}

Awaitable<void> Server::ConfigAPIResource::operator()(Response& response, Request& request) {
    switch (request.getType()) {
        case Request::Type::get:
            response << serverState.requestedConfig.jsonRepresentation;
            break;

        case Request::Type::put:
            co_await serverState.applyConfiguration(
                Config::Root::fromJson(
                    co_await request.readAllAsString()
                )
            );

            // TODO: Write to the config file.
            break;

        default: unreachable();
    }
}
