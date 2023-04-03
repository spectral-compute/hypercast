#pragma once

#include "server/Resource.hpp"
#include "server/ServerState.h"

#include <filesystem>

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
    ~ConfigResource() override;
    ConfigResource(Server::State &state, const std::filesystem::path &configPath) :
        Resource(false), serverState(state), configPath(configPath)
    {
    }

    Awaitable<void> getAsync(Server::Response &response, Server::Request &request) override;
    Awaitable<void> putAsync(Server::Response &response, Server::Request &request) override;

    size_t getMaxPutRequestLength() const noexcept override;

private:
    Server::State &serverState;
    const std::filesystem::path &configPath;
};

} // namespace Api

///@}
