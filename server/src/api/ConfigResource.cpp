#include "ConfigResource.h"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "server/ServerState.h"
#include "util/asio.hpp"
#include "util/File.hpp"

Api::ConfigResource::~ConfigResource() = default;

Awaitable<void> Api::ConfigResource::getAsync(Server::Response &response, Server::Request &request)
{
    response.setCacheKind(Server::CacheKind::none);

    if (!(co_await request.readSome()).empty()) {
        throw Server::Error(Server::ErrorKind::BadRequest);
    }

    response << serverState.requestedConfig.jsonRepresentation;
    co_return;
}

Awaitable<void> Api::ConfigResource::putAsync(Server::Response &response, Server::Request &request)
{
    /* Read the JSON we recieved. */
    std::string json = co_await request.readAllAsString();

    /* Apply the configuration. */
    co_await serverState.applyConfiguration(Config::Root::fromJson(json));

    /* Write the configuration to the configuration file. We only actually get here if the above was successful, because
       failure would throw an exception. This provides protection against writing junk configurations. */
    Util::File configFile(serverState.ioc, configPath, true, false);
    co_await configFile.write(json);
}

size_t Api::ConfigResource::getMaxPutRequestLength() const noexcept
{
    return 1 << 18; // 256 kiB.
}
