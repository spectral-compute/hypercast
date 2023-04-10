#include "ConfigResource.h"

#include "configuration/configuration.hpp"
#include "instance/State.hpp"
#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/File.hpp"

Api::ConfigResource::~ConfigResource() = default;

Awaitable<void> Api::ConfigResource::getAsync(Server::Response &response, Server::Request &request)
{
    response.setCacheKind(Server::CacheKind::none);
    response << serverState.requestedConfig.jsonRepresentation;
    co_return;
}

Awaitable<void> Api::ConfigResource::putAsync(Server::Response &response, Server::Request &request)
{
    /* Read the JSON we recieved. */
    std::string json = co_await request.readAllAsString();

    /* Apply the configuration. */
    try {
        co_await serverState.applyConfiguration(Config::Root::fromJson(json));
    }
    catch (const Config::ParseException &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
    catch (const Instance::BadConfigurationReplacementException &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }

    /* Write the configuration to the configuration file. We only actually get here if the above was successful, because
       failure would throw an exception. This provides protection against writing junk configurations. */
    Util::File configFile(serverState.ioc, configPath, true, false);
    co_await configFile.write(json);
}

size_t Api::ConfigResource::getMaxPutRequestLength() const noexcept
{
    return 1 << 18; // 256 kiB.
}
