#include "SendDataResource.hpp"

#include "dash/DashResources.hpp"
#include "server/Error.hpp"
#include "server/Request.hpp"
#include "util/asio.hpp"
#include "util/json.hpp"

Api::Channel::SendDataResource::~SendDataResource() = default;

Awaitable<void> Api::Channel::SendDataResource::postAsync(Server::Response &, Server::Request &request)
{
    try {
        /* Validate the received JSON object, compact it, and send it to the clients via the interleaves. */
        channel.addControlChunk(Json::dump(Json::parse(co_await request.readAllAsString())),
                                Dash::ControlChunkType::userJsonObject);
    }
    catch (const nlohmann::json::exception &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
}

size_t Api::Channel::SendDataResource::getMaxPostRequestLength() const noexcept
{
    return 1 << 16;
}
