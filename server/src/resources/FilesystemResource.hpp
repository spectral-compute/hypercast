#pragma once

#include "server/CacheKind.hpp"
#include "server/Resource.hpp"
#include "util/Mutex.hpp"

#include <filesystem>

namespace Server
{

class Path;

/**
 * A file-like or directory-like resource whose content comes from the filesystem.
 *
 * Note that this is not atomic in its use of the filesystem, but that should be acceptable for most applications. If
 * it's not, then the user should use a more sophisticated configuration with a reverse HTTP proxy like Nginx.
 */
class FilesystemResource final : public Resource
{
public:
    ~FilesystemResource() override;

    /**
     * Construct a resource to serve a directory from the file system with an index file.
     *
     * @param path The filesystem path to serve content from.
     * @param index The path to use if the path in the request is empty.
     * @param cacheKind The caching to use for the resource.
     * @param isPublic Whether the resource should be available publicly. Only GET can be made public. PUT cannot.
     * @param maxPutSize The maximum size of file that can be PUT into this resource. If zero (the default), PUTting is
     *                   not permitted.
     */
    FilesystemResource(IOContext &ioc, std::filesystem::path path, std::filesystem::path index,
                       CacheKind cacheKind = CacheKind::fixed, bool isPublic = false, size_t maxPutSize = 0) :
        Resource(isPublic),
        ioc(ioc), mutex(ioc), path(std::move(path)), index(std::move(index)), cacheKind(cacheKind),
        maxPutSize(maxPutSize)
    {
    }

    /**
     * Construct a resource to serve a directory from the file system with an index file.
     *
     * @param path The filesystem path to serve content from.
     * @param cacheKind The caching to use for the resource.
     * @param isPublic Whether the resource should be available publicly. Only GET can be made public. PUT cannot.
     * @param maxPutSize The maximum size of file that can be PUT into this resource. If zero (the default), PUTting is
     *                   not permitted.
     */
    FilesystemResource(IOContext &ioc, std::filesystem::path path, CacheKind cacheKind = CacheKind::fixed,
                       bool isPublic = false, size_t maxPutSize = 0) :
        FilesystemResource(ioc, std::move(path), {}, cacheKind, isPublic, maxPutSize)
    {
    }

    Awaitable<void> getAsync(Response &response, Request &request) override;
    Awaitable<void> putAsync(Response &response, Request &request) override;

    bool getAllowNonEmptyPath() const noexcept override;
    size_t getMaxPutRequestLength() const noexcept override;

private:
    /**
     * Gets the full path to a requested path.
     *
     * This handles prepending the base path, and converting to the index path if necessary.
     */
    std::filesystem::path getFullPath(const Path &requestPath) const;

    IOContext &ioc;
    Mutex mutex;
    const std::filesystem::path path;
    const std::filesystem::path index;
    const CacheKind cacheKind;
    const size_t maxPutSize;
};

} // namespace Server
