#include "server/HttpServer.hpp"

#include "resources/ConstantResource.hpp"
#include "server/Request.hpp"
#include "server/Resource.hpp"
#include "server/Response.hpp"

#include "configuration/configuration.hpp"
#include "util/asio.hpp"

#include "log.hpp"

namespace
{

/**
 * Writes the request body to the response.
 */
class EchoResource final : public Server::Resource
{
public:
    EchoResource() : Resource(true) {}

    Awaitable<void> operator()(Server::Response &response, Server::Request &request) override
    {
        response.setCacheKind(Server::CacheKind::none);
        std::string path = request.getPath();
        while (true) {
            std::vector<std::byte> data = co_await request.readSome();
            if (data.empty()) {
                break;
            }
            response << data;
            co_await response.flush();
        }
    }

    bool getAllowGet() const noexcept override { return true; }
};

/**
 * Writes the request body to the response.
 */
class LengthResource final : public Server::Resource
{
public:
    LengthResource() : Resource(true) {}

    Awaitable<void> operator()(Server::Response &response, Server::Request &request) override
    {
        size_t size = 0;
        while (true) {
            std::vector<std::byte> data = co_await request.readSome();
            if (data.empty()) {
                break;
            }
            size += data.size();
        }
        response << std::to_string(size);
    }

    bool getAllowPost() const noexcept override { return true; }
};

/**
 * Emits a short message using chunked encoding.
 */
class ShortChunkResource final : public Server::Resource
{
public:
    ShortChunkResource() : Resource(true) {}

    Awaitable<void> operator()(Server::Response &response, Server::Request &request) override
    {
        response.setCacheKind(Server::CacheKind::ephemeral);
        co_await request.readEmpty();
        response << "Cats";
        co_await response.flush();
        response << " are";
        co_await response.flush();
        response << " cute";
        co_await response.flush();
        response << " :D";
    }

    bool getAllowGet() const noexcept override { return true; }
};

/**
 * Emits a long message using either chunked or non-chunked encoding.
 */
class LongResource final : public Server::Resource
{
public:
    LongResource(bool chunked) : Resource(true), chunked(chunked) {}

    Awaitable<void> operator()(Server::Response &response, Server::Request &request) override
    {
        co_await request.readEmpty();
        for (int i = 0; i < 64; i++) {
            constexpr int count = 1 << 20;
            std::vector<std::byte> chunk(sizeof(int) * count);
            for (int j = 0; j < count; j++) {
                int k = i * count + j;
                memcpy(chunk.data() + j * sizeof(int), &k, sizeof(int));
            }
            response << chunk;
            if (chunked) {
                co_await response.flush();
            }
        }
    }

    bool getAllowGet() const noexcept override { return true; }

private:
    bool chunked;
};

} // namespace

int main()
{
    IOContext ioc;
    ExpectNeverLog log(ioc);
    Server::HttpServer server(ioc, log, 12480, {});
    server.addResource<EchoResource>("Echo");
    server.addResource<LengthResource>("Length");
    server.addResource<LongResource>("Long", false);
    server.addResource<LongResource>("LongChunk", true);
    server.addResource<ShortChunkResource>("ShortChunk");
    server.addResource<Server::ConstantResource>("Short", "Cats are cute :D", "text/plain");
    ioc.run();
}