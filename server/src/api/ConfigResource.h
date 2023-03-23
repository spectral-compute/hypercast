#pragma once

#include "server/Resource.hpp"
#include "server/ServerState.h"

namespace Server {


// A resource for querying or modifying the configuration.
class ConfigAPIResource : public Resource {
    State &serverState;

public:
    ConfigAPIResource(State& state);

    Awaitable<void> operator()(Response &response, Request &request) override;
};


} // namespace Server
