#pragma once

#include "server/SynchronousResource.hpp"

#include <map>
#include <string>

namespace Config
{

struct Channel;

} // namespace Config

namespace Instance
{

/**
 * Provides an index of all the channels the server is streaming.
 */
class ChannelsIndexResource final : public Server::SynchronousNullaryResource
{
public:
    ~ChannelsIndexResource() override;
    explicit ChannelsIndexResource(const std::map<std::string, Config::Channel> &channels) : channels(channels) {}

    void getSync(Server::Response &response, const Server::Request &request) override;

private:
    const std::map<std::string, Config::Channel> &channels;
};

} // namespace Instance
