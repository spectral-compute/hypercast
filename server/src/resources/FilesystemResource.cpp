#include "FilesystemResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/File.hpp"
#include "util/subprocess.hpp"

#include <memory>

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
    std::filesystem::path filePath = getFullPath(request.getPath());

    /* We need a mutex if we can PUT so there isn't a race condition. */
    auto lockGuard = maxPutSize ? std::make_unique<Mutex::LockGuard>(co_await mutex.lockGuard()) : nullptr;

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

Awaitable<void> Server::FilesystemResource::putAsync(Response &response, Request &request)
{
    /* Reject PUT requests if we're not allowing them. */
    if (!maxPutSize) {
        throw Error(ErrorKind::UnsupportedType);
    }

    /* Figure out the path. */
    std::filesystem::path filePath = getFullPath(request.getPath());

    /* Stop concurrent operations. */
    auto lockGuard = co_await mutex.lockGuard();

    /* Check that the path either doesn't exist, or is not a directory. */
    if (std::filesystem::exists(filePath) && std::filesystem::is_directory(filePath)) {
        throw Error(ErrorKind::Conflict);
    }

    /* Create parent directories if necessary. */
    std::filesystem::create_directories(filePath.parent_path());

    /* Write the file contents. */
    Util::File file(ioc, std::move(filePath), true, false);
    while (true) {
        std::vector<std::byte> data = co_await request.readSome();
        if (data.empty()) {
            co_return;
        }
        co_await file.write(data);
    }
}

bool Server::FilesystemResource::getAllowNonEmptyPath() const noexcept
{
    return true;
}

size_t Server::FilesystemResource::getMaxPutRequestLength() const noexcept
{
    return maxPutSize;
}

std::filesystem::path Server::FilesystemResource::getFullPath(const Path &requestPath) const
{
    // This is protected from directory traversal attacks by the constructor for the object returned by
    // request.getPath().
    return (requestPath.empty() && !index.empty()) ? path / index : (path / requestPath);
}
