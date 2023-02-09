#include "Response.hpp"

#include <util/asio.hpp>

Server::Response::~Response() = default;

void Server::Response::setErrorAndMessage(ErrorKind kind, std::string_view message)
{
    setError(kind);
    setMimeType(message.empty() ? "" : "text/plain");
    if (!message.empty()) {
        (*this) << message;
    }
}

Awaitable<void> Server::Response::flush(bool end)
{
    Awaitable<void> result = flushBody(end);
    writeStarted = true; // We've now started writing. This is after waitBody() so it can detect the first write.
    return result;
}
