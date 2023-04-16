#include "BlankResource.hpp"

#include "ffmpeg/zmqsend.hpp"
#include "server/Error.hpp"
#include "server/Request.hpp"
#include "util/asio.hpp"
#include "util/json.hpp"

namespace Api::Channel
{

void from_json(const nlohmann::json &j, BlankResource::RequestObject &out)
{
    Json::ObjectDeserializer d(j);
    d(out.blank, "blank", true);
    d();
}

} // namespace Api::Channel

Api::Channel::BlankResource::~BlankResource() = default;

Awaitable<void> Api::Channel::BlankResource::handleRequest(const RequestObject &request)
{
    co_await Ffmpeg::zmqsend(ioc, address, {
        { "vblank", "enable", request.blank ? "1" : "0" },
        { "ablank", "enable", request.blank ? "1" : "0" }
    });
}

Awaitable<void> Api::Channel::BlankResource::postAsync(Server::Response &, Server::Request &request)
{
    try {
        co_await handleRequest(Json::parse(co_await request.readAllAsString()).get<RequestObject>());
    }
    catch (const nlohmann::json::exception &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
    catch (const Json::ObjectDeserializer::Exception &e) {
        throw Server::Error(Server::ErrorKind::BadRequest, e.what());
    }
}

size_t Api::Channel::BlankResource::getMaxPostRequestLength() const noexcept
{
    return 256;
}
