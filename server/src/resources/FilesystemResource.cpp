#include "FilesystemResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/File.hpp"
#include "util/subprocess.hpp"

namespace
{

/**
 * The set of built-in known MIME types.
 *
 * This is useful because some important text-based formats are not always correctly identified by file.
 */
constexpr std::pair<std::string_view, std::string_view> mimeTypes[] = {
    { "css", "text/css" },
    { "html", "text/html" },
    { "js", "text/javascript" },
    { "json", "application/json" },
    { "svg", "image/svg+xml" }
};

/**
 * Get the MIME type of a file.
 */
Awaitable<std::string> getMimeTypeForFile(IOContext &ioc, const std::filesystem::path &path)
{
    /* See if we know about the MIME type by the filename's extension. */
    std::string extension = path.extension().string().substr(1);
    for (auto [candidateExtension, candidateMimeType]: mimeTypes) {
        if (candidateExtension == extension) {
            co_return std::string(candidateMimeType);
        }
    }

    /* Otherwise, use file to identify the MIME type. */
    std::string mimeType = co_await Subprocess::getStdout(ioc, "file", {"-bEL", "--mime-type", path.string()});
    if (!mimeType.empty() && mimeType.back() == '\n') {
        mimeType.resize(mimeType.size() - 1);
    }
    co_return mimeType;
}

} // namespace

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
    response.setMimeType(co_await getMimeTypeForFile(ioc, filePath));

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
