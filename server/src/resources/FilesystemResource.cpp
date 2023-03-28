#include "FilesystemResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/File.hpp"
#include "util/subprocess.hpp"

Server::FilesystemResource::~FilesystemResource() = default;

Awaitable<void> Server::FilesystemResource::getAsync(Response &response, Request &request)
{
    /* Set up some response properties. */
    response.setCacheKind(cacheKind);

    /* Figure out the path. */
    // This is protected from directory traversal attacks by the constructor for the object returned by
    // request.getPath().
    std::filesystem::path filePath = (request.getPath().empty() && !index.empty()) ?
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
    Util::File file(ioc, std::move(filePath));
    while (true) {
        std::vector<std::byte> data = co_await file.readSome();
        if (data.empty()) {
            co_return;
        }
        response << std::move(data);
    }
}

bool Server::FilesystemResource::getAllowNonEmptyPath() const noexcept
{
    return true;
}
