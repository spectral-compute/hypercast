#pragma once

#ifndef NDEBUG

#include "server/SynchronousResource.hpp"

namespace Config
{

class Root;

} // namespace Config

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
    FullConfigResource(const Config::Root &config) : config(config) {}

    void getSync(Server::Response &response, const Server::Request &request) override;

private:
    const Config::Root &config;
};

} // namespace Api

#endif // NDEBUG
