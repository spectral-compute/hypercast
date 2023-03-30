#include "ConfigResource.h"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/debug.hpp"
#include "util/asio.hpp"

Api::ConfigResource::ConfigResource(Server::State &state) : Resource(false), serverState(state) {}

Awaitable<void> Api::ConfigResource::operator()(Server::Response &response, Server::Request &request)
{
    switch (request.getType()) {
        case Server::Request::Type::get:
            response << serverState.requestedConfig.jsonRepresentation;
            break;

        case Server::Request::Type::put:
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
