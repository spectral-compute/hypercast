#pragma once

#include "server/Resource.hpp"

class IOContext;

namespace Dash
{

class DashResources;

} // namespace Dash

namespace Api::Channel
{

/**
 * Sends a JSON object to the client, marked as being for the user application.
 */
class SendDataResource final : public Server::Resource
{
public:
    ~SendDataResource() override;

    /**
     * Constructor :)
     *
     * @param channel The Dash::DashResources object for the channel this resource is for.
     */
    SendDataResource(Dash::DashResources &channel) : channel(channel) {}

    Awaitable<void> postAsync(Server::Response &response, Server::Request &request) override;
    size_t getMaxPostRequestLength() const noexcept override;

private:
    Dash::DashResources &channel;
};

} // namespace Api::Channel
