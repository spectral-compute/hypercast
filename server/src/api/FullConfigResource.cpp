#ifndef NDEBUG

#include "FullConfigResource.hpp"

#include "configuration/configuration.hpp"
#include "server/Response.hpp"
#include "server/ServerState.h"

Api::FullConfigResource::~FullConfigResource() = default;

void Api::FullConfigResource::getSync(Server::Response &response, const Server::Request &)
{
    response << state.getConfiguration().toJson();
}

#endif // NDEBUG
