#pragma once

#ifndef NDEBUG

#include "server/SynchronousResource.hpp"

namespace Server
{

struct State;

} // namespace Server

namespace Api
{

/**
 * Get the full configuration with default filled in.
 *
 * This is useful for development purposes: for debugging the defaults system. Maybe one day, it'll also be useful for
 * the UI?
 */
class FullConfigResource final : public Server::SynchronousNullaryResource
{
public:
    ~FullConfigResource() override;
    FullConfigResource(Server::State &state) : state(state) {}

    void getSync(Server::Response &response, const Server::Request &request) override;

private:
    Server::State &state;
};

} // namespace Api

#endif // NDEBUG
