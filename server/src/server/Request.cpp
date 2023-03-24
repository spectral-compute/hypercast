#include "Request.hpp"

#include "Error.hpp"

#include "util/asio.hpp"
#include "util/util.hpp"

Server::Request::~Request() = default;

Awaitable<std::vector<std::byte>> Server::Request::readSome()
{
    std::vector<std::byte> data = co_await doReadSome();
    bytesRead += (int) data.size();
    checkMaxLength();
    co_return data;
}

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

int Server::Request::getBytesRead() const
{
    return bytesRead;
}

void Server::Request::checkMaxLength() const
{
    if (bytesRead > maxLength) {
        throw Error(ErrorKind::BadRequest, "Request body too long (got >=" + std::to_string(bytesRead) + " bytes, but the limit is " + std::to_string(maxLength) + ")");
    }

}

void Server::Request::setMaxLength(int l)
{
    maxLength = l;
    checkMaxLength();
}
