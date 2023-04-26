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
 * Sends generic data to the client.
 */
class SendDataResource final : public Server::Resource
{
public:
    /**
     * The kind of data to send.
     */
    enum class Kind
    {
        /**
         * A JSON object for use by the client library's user.
         */
        userJson,

        /**
         * Binary data for use by the client library's user.
         */
        userBinary,

        /**
         * A UTF-8 encoded string for use by the client library's user.
         */
        userString
    };

    ~SendDataResource() override;

    /**
     * Constructor :)
     *
     * @param channel The Dash::DashResources object for the channel this resource is for.
     * @param kind The kind of data to send to the client.
     */
    SendDataResource(Dash::DashResources &channel, Kind kind) : channel(channel), kind(kind) {}

    Awaitable<void> postAsync(Server::Response &response, Server::Request &request) override;
    size_t getMaxPostRequestLength() const noexcept override;

private:
    Dash::DashResources &channel;
    const Kind kind;
};

} // namespace Api::Channel
