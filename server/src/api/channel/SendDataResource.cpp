#include "SendDataResource.hpp"

#include "dash/DashResources.hpp"
#include "server/Error.hpp"
#include "server/Request.hpp"
#include "util/asio.hpp"
#include "util/debug.hpp"
#include "util/json.hpp"

namespace
{

Dash::ControlChunkType getChunkKind(Api::Channel::SendDataResource::Kind kind)
{
    switch (kind) {
        case Api::Channel::SendDataResource::Kind::userJson: return Dash::ControlChunkType::userJsonObject;
        case Api::Channel::SendDataResource::Kind::userBinary: return Dash::ControlChunkType::userBinaryData;
        case Api::Channel::SendDataResource::Kind::userString: return Dash::ControlChunkType::userString;
    }
    unreachable();
}

} // namespace

Api::Channel::SendDataResource::~SendDataResource() = default;

Awaitable<void> Api::Channel::SendDataResource::postAsync(Server::Response &, Server::Request &request)
{
    std::vector<std::byte> data = co_await request.readAll();
    Dash::ControlChunkType chunkKind = getChunkKind(kind);

    /* Decode/re-encode (if necessary) the data and send it to the client. */
    switch (kind) {
        case Kind::userJson: {
            try {
                // Validate the received JSON object, compact it, and send it to the clients via the interleaves.
                std::string_view string((const char *)data.data(), data.size());
                channel.addControlChunk(Json::dump(Json::parse(string)), chunkKind);
                co_return;
            }
            catch (const nlohmann::json::exception &e) {
                throw Server::Error(Server::ErrorKind::BadRequest, e.what());
            }
        }
        case Kind::userBinary:
        case Kind::userString: // TODO: One day, maybe validate that the string really is UTF-8.
            // Just pass the binary data straight through.
            channel.addControlChunk(std::move(data), chunkKind);
            co_return;
    }
    unreachable();
}

size_t Api::Channel::SendDataResource::getMaxPostRequestLength() const noexcept
{
    return 1 << 16;
}
