#include "Request.hpp"

#include "Error.hpp"

#include "util/asio.hpp"
#include "util/util.hpp"

Server::Request::~Request() = default;

Awaitable<std::vector<std::byte>> Server::Request::readAll()
{
    /* Extract all the data. */
    std::vector<std::vector<std::byte>> dataParts;
    while (true) {
        std::vector<std::byte> data = co_await readSome();
        if (data.empty()) {
            break;
        }
        dataParts.emplace_back(std::move(data));
    }

    /* Concatenate and return. */
    co_return Util::concatenate(std::move(dataParts));
}

Awaitable<std::string> Server::Request::readAllAsString()
{
    std::vector<std::byte> bytes = co_await readAll();
    co_return std::string((const char *)bytes.data(), bytes.size());
}

Awaitable<void> Server::Request::readEmpty()
{
    if (!(co_await readAll()).empty()) {
        throw Error(ErrorKind::BadRequest, "Request not empty.");
    }
}
