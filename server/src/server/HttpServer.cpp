#include "HttpServer.hpp"

#include "Address.hpp"
#include "CacheKind.hpp"
#include "Error.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include "configuration/configuration.hpp"
#include "log/Log.hpp"
#include "util/asio.hpp"
#include "util/util.hpp"

#include <chrono>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/write.hpp>

#include "util/debug.hpp"

/// @addtogroup server_implementation
/// @{

namespace
{

/**
 * Format an endpoint to a human-readable string.
 */
std::string formatEndpoint(const boost::asio::ip::tcp::endpoint &endpoint)
{
    return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

/**
 * Implementation of Server::HttpServer::Connection, but as a base class so it can be used by the other HTTP classes,
 * while still allowing it to be used by methods of Server::HttpServer without putting lots of boost stuff in its
 * header.
 */
struct Connection
{
    explicit Connection(boost::asio::ip::tcp::socket socket) : socket(std::move(socket)) {}

    boost::asio::ip::tcp::socket socket;
    boost::beast::flat_buffer buffer;
};

/**
 * Wraps the boost::beast and boost::asio stuff into a Server::Request.
 */
class HttpRequest final : public Server::Request
{
public:
    ~HttpRequest() override = default;

    /**
     * Constructor :)
     *
     * @param parser The HTTP parser. This is created outside the Request class because we need to parse the header
     *               before the Request object can be created.
     * @param connection The connection to read the body from.
     * @param path The request's resource path.
     * @param type The request type. This is separate as well in case the request type is not valid.
     * @param isPublic Whether the request came from a public location (e.g: the Internet).
     */
    explicit HttpRequest(boost::beast::http::request_parser<boost::beast::http::buffer_body> &parser,
                         Connection &connection, Server::Path path, Type type, bool isPublic) :
        Request(std::move(path), type, isPublic), parser(parser), connection(connection)
    {
    }

    Awaitable<std::vector<std::byte>> doReadSome() override
    {
        /* Keep trying to read something until we get a non-empty result or end of body. */
        while (!parser.is_done()) {
            // Allocate a buffer if one doesn't already exist.
            if (buffer.empty()) {
                buffer = std::vector<std::byte>(1 << 16);
            }

            // Read some data from the request body.
            boost::beast::http::buffer_body::value_type &body = parser.get().body();
            body.data = buffer.data();
            body.size = buffer.size();
            co_await boost::beast::http::async_read_some(connection.socket, connection.buffer, parser,
                                                         boost::asio::use_awaitable);
            size_t readBodySize = buffer.size() - body.size;

            // Don't return a zero-length read, which boost::beast::http::async_read_some can sometimes falsely emit.
            if (readBodySize == 0) {
                continue;
            }

            // Return the buffer by moving if most of it was used.
            // The idea is to avoid the copy. If most of it is unused, then it could be inefficient to the entire memory
            // allocation for just a small amount of data.
            if (readBodySize >= buffer.size() / 2) {
                buffer.resize(readBodySize);
                co_return std::move(buffer);
            }

            // Return a copy of the data in the buffer.
            // This keeps the original buffer for reuse.
            std::vector<std::byte> result;
            result.insert(result.end(), buffer.begin(), buffer.begin() + readBodySize);
            co_return result;
        }

        /* Don't keep reading once we've read the end of the body. */
        co_return std::vector<std::byte>{};
    }

private:
    boost::beast::http::request_parser<boost::beast::http::buffer_body> &parser;
    Connection &connection;

    /**
     * A buffer into which data can be read.
     *
     * By making this a std::vector, we achieve two things.
     * 1. It can be returned from readSome by moving, rather than by copying.
     * 2. It can be large without overflowing the stack.
     *
     * By making this a member variable, it can be shared across multiple reads if those reads are short.
     */
    std::vector<std::byte> buffer;
};

/**
 * Wraps the boost::beast and boost::asio stuff into a Server::Response.
 */
class HttpResponse final : public Server::Response
{
public:
    ~HttpResponse() override = default;

    /**
     * Constructor :)
     *
     * @param connection The connection to write the response to.
     * @param keepAlive Whether to instruct the client to keep the connection alive after the response body has been
     *                  sent.
     * @param discard Whether to discard the message body. This is useful for implementing HEAD.
     * @param methodAllowsCaching Whether the HTTP method we're responding to allows caching.
     */
    explicit HttpResponse(Connection &connection, const Config::Http &httpConfig, bool keepAlive, bool discard,
                          bool methodAllowsCaching) :
        connection(connection), httpConfig(httpConfig), discard(discard), methodAllowsCaching(methodAllowsCaching)
    {
        response.keep_alive(keepAlive);
    }

    operator bool() const
    {
        return !(bool)getErrorKind();
    }

private:
    void writeBody(std::vector<std::byte> data) override
    {
        /* Put the data into the queue. Actual writing happens when wait is called. */
        bodyQueue.emplace_back(std::move(data));
    }

    Awaitable<void> flushBody(bool end) override
    {
        /* Get the new body data to send. */
        std::vector<std::byte> data = Util::concatenate(std::move(bodyQueue));

        /* If we haven't already sent the headers, send them. */
        if (!serializer.is_header_done()) {
            co_await transmitHeaders(end ? std::optional(data.size()) : std::nullopt);
        }

        /* HEAD requests don't *actually* send the body data. Also, don't bother writing anything if there's no data. */
        if (discard) {
            co_return;
        }

        /* Send the body data. */
        if (response.chunked()) {
            // Chunked encode doesn't seem to like having boost::beast::http::async_write called multiple times, even
            // with response.body().more set to true, so we write the chunks to the socket directly, as in the
            // documentation's examples:
            // https://www.boost.org/doc/libs/1_81_0/libs/beast/doc/html/beast/using_http/chunked_encoding.html
            if (!data.empty()) {
                boost::asio::const_buffer buffer(data.data(), data.size());
                co_await boost::asio::async_write(connection.socket, boost::beast::http::make_chunk(buffer),
                                                  boost::asio::use_awaitable);
            }
            if (end) {
                co_await boost::asio::async_write(connection.socket, boost::beast::http::make_chunk_last(),
                                                  boost::asio::use_awaitable);
            }
        }
        else {
            response.body().data = data.data();
            response.body().size = data.size();
            response.body().more = false; // We have the entire message here.
            co_await boost::beast::http::async_write(connection.socket, serializer, boost::asio::use_awaitable);
        }
    }

    std::string paddedNum(unsigned d) {
        if (d < 10) {
            return "0" + std::to_string(d);
        }

        return std::to_string(d);
    }

    std::string computeDateHeader() {
        using namespace std::chrono;
        auto tp = system_clock::now();
        auto dp = floor<days>(tp);
        year_month_day ymd{dp};
        weekday wd{dp};
        hh_mm_ss time{floor<milliseconds>(tp-dp)};
        int y = (int) ymd.year();
        month m = ymd.month();
        auto d = ymd.day();
        auto h = (int) time.hours().count();
        auto M = (int) time.minutes().count();
        auto s = (int) time.seconds().count();

        std::stringstream out;
        if (wd == Sunday) {
            out << "Sun";
        } else if (wd == Monday) {
            out << "Mon";
        } else if (wd == Tuesday) {
            out << "Tue";
        } else if (wd == Wednesday) {
            out << "Wed";
        } else if (wd == Thursday) {
            out << "Thu";
        } else if (wd == Friday) {
            out << "Fri";
        } else if (wd == Saturday) {
            out << "Sat";
        }

        out << ", ";
        out << paddedNum((unsigned) d);
        out << " ";
        if (m == January) {
            out << "Jan";
        } else if (m == February) {
            out << "Feb";
        } else if (m == March) {
            out << "Mar";
        } else if (m == April) {
            out << "Apr";
        } else if (m == May) {
            out << "May";
        } else if (m == June) {
            out << "Jun";
        } else if (m == July) {
            out << "Jul";
        } else if (m == August) {
            out << "Aug";
        } else if (m == September) {
            out << "Sep";
        } else if (m == October) {
            out << "Oct";
        } else if (m == November) {
            out << "Nov";
        } else if (m == December) {
            out << "Dec";
        }
        out << " " << y << " ";

        out << paddedNum((unsigned) h) << ":" << paddedNum((unsigned) M) << ":" << paddedNum((unsigned) s) << " GMT";
        return out.str();
    }

    Awaitable<void> transmitHeaders(std::optional<size_t> contentLength)
    {
        /* Set the response code. */
        response.result(getHttpStatusCode());

        /* Set the server string. */
        response.set(boost::beast::http::field::server, "Spectral Compute Ultra Low Latency Video Streamer");

        // The Date header.
        response.set(boost::beast::http::field::date, computeDateHeader());

        /* Set cache control. */
        if (methodAllowsCaching) {
            int cacheDuration = getCacheDuration();
            response.set(boost::beast::http::field::cache_control,
                         cacheDuration ? "public, max-age=" + std::to_string(cacheDuration) : "no-cache");
        }

        /* Set other headers. */
        if (httpConfig.origin) {
            response.set(boost::beast::http::field::access_control_allow_origin, *httpConfig.origin);
        }
        if (!getMimeType().empty()) {
            response.set(boost::beast::http::field::content_type, getMimeType());
        }

        for (const auto& p : this->extraHeaders) {
            response.set(p.first, p.second);
        }

        /* If we've not transmitted the headers, but we're guaranteeing no more data in the body, we can set the
           content length. Otherwise, we need to use chunked encoding. */
        if (contentLength) {
            response.content_length(*contentLength);
        }
        else {
            response.chunked(true);
        }

        /* Transmit the headers. */
        co_await boost::beast::http::async_write_header(connection.socket, serializer, boost::asio::use_awaitable);
        assert(serializer.is_header_done());
    }

    /**
     * Get the HTTP status code for the response.
     */
    boost::beast::http::status getHttpStatusCode() const
    {
        if (!getErrorKind()) {
            return boost::beast::http::status::ok;
        }
        switch (*getErrorKind()) {
            case Server::ErrorKind::BadRequest: return boost::beast::http::status::bad_request;
            case Server::ErrorKind::Forbidden: return boost::beast::http::status::forbidden;
            case Server::ErrorKind::NotFound: return boost::beast::http::status::not_found;
            case Server::ErrorKind::UnsupportedType: return boost::beast::http::status::method_not_allowed;
            case Server::ErrorKind::Conflict: return boost::beast::http::status::conflict;
            case Server::ErrorKind::Internal: return boost::beast::http::status::internal_server_error;
        }
        unreachable();
    }

    /**
     * Get the cache duration in seconds for the response.
     */
    int getCacheDuration() const
    {
        switch (getCacheKind()) {
            case Server::CacheKind::none: return 0;
            case Server::CacheKind::ephemeral: return 1;
            case Server::CacheKind::fixed: return httpConfig.cacheNonLiveTime;
            case Server::CacheKind::indefinite: return 1 << 30;
        }
        unreachable();
    }

    Connection &connection;
    const Config::Http &httpConfig;

    /**
     * Whether to discard the message body rather than send it to the client.
     *
     * This is useful for implementing HEAD.
     */
    const bool discard = false;

    /**
     * Whether the method in the request that this is a response to permits caching.
     */
    const bool methodAllowsCaching;

    boost::beast::http::response<boost::beast::http::buffer_body> response;
    boost::beast::http::response_serializer<boost::beast::http::buffer_body> serializer{response};
    std::vector<std::vector<std::byte>> bodyQueue;
};

/**
 * Convert a Boost Beast HTTP verb into a generic request type.
 *
 * @return The request type corresponding to the verb, or std::nullopt if there is none.
 */
std::optional<Server::Request::Type> getRequestType(boost::beast::http::verb verb)
{
    switch (verb) {
        case boost::beast::http::verb::head:
        case boost::beast::http::verb::get:
            return Server::Request::Type::get;
        case boost::beast::http::verb::post:
            return Server::Request::Type::post;
        case boost::beast::http::verb::put:
            return Server::Request::Type::put;
        case boost::beast::http::verb::options:
            return Server::Request::Type::options;
        default: break;
    }
    return std::nullopt;
}

} // namespace

/// @}

/**
 * Connection argument to pass around to (private) methods defined in the header :)
 */
struct Server::HttpServer::Connection final : public ::Connection
{
    using ::Connection::Connection;
};

Server::HttpServer::~HttpServer() = default;

Server::HttpServer::HttpServer(IOContext &ioc, Log::Log &log, const Config::Network &networkConfig,
                               const Config::Http &httpConfig) :
    Server(log), ioc(ioc), networkConfig(networkConfig), httpConfig(httpConfig)
{
    Log::Context listenContext = log("listen");

    /* Start a coroutine in the IO context, so we can return immediately. */
    spawnDetached(ioc, listenContext, [this]() -> Awaitable<void> { return listen(); }, Log::Level::fatal);
}

Awaitable<bool> Server::HttpServer::onRequest(Connection &connection)
{
    /* Figure out whether the source is public or not. */
    // Get an address to compare to.
    boost::asio::ip::address_v6::bytes_type remoteAddressBytes =
        connection.socket.remote_endpoint().address().to_v6().to_bytes();
    ::Server::Address remoteAddress({(const std::byte *)remoteAddressBytes.data(), remoteAddressBytes.size()});

    // Figure out if any of the private networks contain the remote address.
    bool isPublic = !remoteAddress.isLoopback();
    if (isPublic) {
        for (const ::Server::Address &network: networkConfig.privateNetworks) {
            if (network.contains(remoteAddress)) {
                isPublic = false;
                break;
            }
        }
    }

    /* Read the HTTP header. */
    boost::beast::http::request_parser<boost::beast::http::buffer_body> parser;
    parser.header_limit(1 << 20); // Plenty of space for headers.
    parser.body_limit((size_t)1 << 32); // We might receive large files.
    try {
        co_await boost::beast::http::async_read_header(connection.socket, connection.buffer, parser,
                                                       boost::asio::use_awaitable);
    }
    catch (const boost::system::system_error &e) {
        if (e.code() == boost::beast::http::error::end_of_stream && !parser.got_some()) {
            co_return false;
        }
        throw;
    }
    assert(parser.is_header_done());

    /* Create a Response object. Create this earlier, so we can use its error transmission code. */
    HttpResponse response(connection, httpConfig, parser.keep_alive(),
                          parser.get().method() == boost::beast::http::verb::head,
                          parser.get().method() == boost::beast::http::verb::head ||
                              parser.get().method() == boost::beast::http::verb::get);

    /* Create a Request object to handle the request body. */
    // Figure out the request type.
    std::optional<Request::Type> requestType = getRequestType(parser.get().method());
    if (!requestType) {
        response.setErrorAndMessage(ErrorKind::UnsupportedType);
        co_await response.flush(true);

        // We didn't handle the input, so the TCP stream will now be messed up.
        connection.buffer.clear(); // Don't generate an error about excess data in the buffer.
        co_return false;
    }

    // Figure out the resource path.
    std::optional<Path> path;
    try {
        path = Path(std::string_view(parser.get().target().data(), parser.get().target().size()));
    }
    catch (const Path::Exception &) {}
    if (!path) {
        response.setErrorAndMessage(ErrorKind::Forbidden);
        co_await response.flush(true); // Annoyingly, this cannot be in the catch block.

        // We didn't handle the input, so the TCP stream will now be messed up.
        connection.buffer.clear(); // Don't generate an error about excess data in the buffer.
        co_return false;
    }

    // Actually create the request.
    HttpRequest request(parser, connection, std::move(*path), *requestType, isPublic);

    /* Check the request, find the resource to handle it, get it to service the request, and wait for the response to
       be written to the network (or at least to some "low water line" buffer level). */
    co_await (*this)(response, request);

    /* If the body hasn't been fully read, then its contents (or absence) can't have been checked, which is not
       ideal. The real badness, though, is that the TCP stream may still have data for the request in its buffer,
       which would confuse further message processing on the same stream. Throwing here:
       1. Triggers any logging we might have turned on.
       2. Prevents the socket from being used for the next request.
       This is reachable if readEmpty was called, but there was actually request data. */
    if (!(co_await request.readSome()).empty()) {
        // If there's an error, then we'd expect the end of request body might not have been reached.
        if (!response) {
            connection.buffer.clear(); // Don't generate an error about excess data in the buffer.
            co_return false;
        }

        // Otherwise, this is incorrect.
        throw std::runtime_error("End of request body not reached.");
    }

    /* Tell the caller if the connection can accept another request. */
    co_return parser.keep_alive();
}

Awaitable<void> Server::HttpServer::onConnection(Connection &connection)
{
    Log::Context connectionContext = log("connection");

    /* Keep handling requests until either an exception happens or onRequest returns false. */
    try {
        connectionContext << "endpoints" << Log::Level::info
                          << formatEndpoint(connection.socket.remote_endpoint()) << " -> "
                          << formatEndpoint(connection.socket.local_endpoint());

        // Keep handling requests while we're allowed to keep the connection alive.
        while (co_await onRequest(connection)) {}

        // See if we had stuff at the end of the stream. If so, that's an error.
        if (connection.buffer.size() > 0) {
            throw std::runtime_error("Excess data in buffer after handling request.");
        }
    }
    catch (const std::exception &e) {
        connectionContext << Log::Level::error << "Exception while handling request: " << e.what() << ".";
    }
    catch (...) {
        connectionContext << Log::Level::error << "Unknown exception while handling request.";
    }

    /* Close the socket. */
    try {
        connection.socket.close();
    }
    catch (const std::exception &e) {
        connectionContext << Log::Level::error << "Exception while closing socket: " << e.what() << ".";
    }
    catch (...) {
        connectionContext << Log::Level::error << "Unknown exception while closing socket.";
    }
}

Awaitable<void> Server::HttpServer::listen()
{
    Log::Context connectionContext = log("acceptor");

    /* Listen for connections. */
    boost::asio::ip::tcp::acceptor acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(),
                                                                                networkConfig.port));

    /* Handle each connection. */
    while (true) {
        try {
            // Accept a connection.
            boost::asio::ip::tcp::socket socket = co_await acceptor.async_accept(boost::asio::use_awaitable);

            // Spawn a detached coroutine to handle the socket, so we can get back to accepting more connections.
            spawnDetached(ioc, [this, socket = std::move(socket)]() mutable -> Awaitable<void> {
                Connection connection(std::move(socket));
                co_await onConnection(connection);
            });
        }
        catch (const std::exception &e) {
            connectionContext << Log::Level::error << "Exception while accepting connection: " << e.what() << ".";
        }
        catch (...) {
            connectionContext << Log::Level::error << "Unknown exception while accepting connection.";
        }
    }
}
