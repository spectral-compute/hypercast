#include "FilesystemResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/subprocess.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/stream_file.hpp>

Server::FilesystemResource::~FilesystemResource() = default;

Awaitable<void> Server::FilesystemResource::getAsync(Response &response, Request &request)
{
    /* Set up some response properties. */
    response.setCacheKind(cacheKind);

    /* Figure out the path. */
    // This is protected from directory traversal attacks by the constructor for the object returned by
    // request.getPath().
    Path requestPath = request.getPath();
    std::filesystem::path filePath = (requestPath.empty() && !index.empty()) ?
                                     path / index : (path / request.getPath());

    /* Check that the path exists and that it's not a directory. */
    // Unfortunately, boost.asio doesn't seem to have anything that would do this.
    if (!std::filesystem::exists(filePath)) {
        throw Error(ErrorKind::NotFound);
    }
    if (std::filesystem::is_directory(filePath)) {
        throw Error(ErrorKind::Forbidden);
    }

    /* Set the MIME type. */
    {
        std::string mimeType = co_await Subprocess::getStdout(ioc, "file", {"-bEL", "--mime-type", filePath.string()});
        if (!mimeType.empty() && mimeType.back() == '\n') {
            mimeType.resize(mimeType.size() - 1);
        }
        response.setMimeType(std::move(mimeType));
    }

    /* Write the file to the response. */
    boost::asio::stream_file f(ioc, filePath, boost::asio::stream_file::read_only);
    while (true) {
        std::byte buffer[4096];
        auto [e, n] = co_await f.async_read_some(boost::asio::buffer(buffer),
                                                 boost::asio::as_tuple(boost::asio::use_awaitable));
        if (n > 0) {
            response << std::span(buffer, n);
        }

        if (e == boost::asio::error::eof) {
            f.close();
            co_return;
        }
        else if (e) {
            throw std::runtime_error("Error reading file " + filePath.string() + ": " + e.message());
        }
    }
}

bool Server::FilesystemResource::getAllowNonEmptyPath() const noexcept
{
    return true;
}
