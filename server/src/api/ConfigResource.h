#pragma once

#include "server/Resource.hpp"
#include "server/ServerState.h"

/**
 * @defgroup api API
 *
 * Stuff (e.g: resources) that implement the API that the server exposes via HTTP.
 */
/// @addtogroup api
/// @{

/**
 * Stuff (e.g: resources) that implement the API that the server exposes via HTTP.
 */
namespace Api
{

// A resource for querying or modifying the configuration.
class ConfigResource final : public Server::Resource
{
public:
    ConfigResource(Server::State &state);

    Awaitable<void> operator()(Server::Response &response, Server::Request &request) override;

private:
    Server::State &serverState;
};

} // namespace Api

///@}
