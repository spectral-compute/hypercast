#pragma once

#include "Server.hpp"
#include "util/awaitable.hpp"

class IOContext;

namespace Config
{

struct Http;
struct Network;

} // namespace Config

namespace Server
{

/**
 * An HTTP server.
 */
class HttpServer final : public Server
{
public:
    ~HttpServer() override;

    /**
     * Create the HTTP server.
     *
     * @note The server never terminates.
     *
     * @param ioc The server is created as a detached coroutine in this context.
     * @param networkConfig The server's network configuration object.
     * @param httpConfig The server's HTTP configuration object.
     */
    explicit HttpServer(IOContext &ioc, Log::Log &log, const Config::Network &networkConfig,
                        const Config::Http &httpConfig);

private:
    /**
     * Low level objects about a TCP connection.
     */
    struct Connection;

    /**
     * Handle a request from a connection.
     *
     * @param connection The connection that originated the request.
     * @return True if the connection should remain open.
     */
    Awaitable<bool> onRequest(Connection &connection);

    /**
     * Called for every connection that's established.
     */
    Awaitable<void> onConnection(Connection &connection);

    /**
     * Called by the constructor as a worker coroutine.
     *
     * @param port The TCP port to listen on.
     * @param allowPrivate Whether to allow access to private resources (when the source is appropriate).
     */
    Awaitable<void> listen(int16_t port, bool allowPrivate);

    IOContext &ioc;
    const Config::Network &networkConfig;
    const Config::Http &httpConfig;

    /**
     * A log context that needs to survive the constructor.
     */
    Log::Context listenContext;
};

} // namespace Server
