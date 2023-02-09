#pragma once

#include "server/CacheKind.hpp"
#include "server/Resource.hpp"

#include <filesystem>

namespace Server
{

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
     * @param isPublic Whether the resource should be available publicly.
     */
    FilesystemResource(IOContext &ioc, std::filesystem::path path, std::filesystem::path index,
                       CacheKind cacheKind = CacheKind::fixed, bool isPublic = false) :
        Resource(isPublic), ioc(ioc), path(std::move(path)), index(std::move(index)), cacheKind(cacheKind)
    {
    }
    /**
     * Construct a resource to serve a directory from the file system with an index file.
     *
     * @param path The filesystem path to serve content from.
     * @param cacheKind The caching to use for the resource.
     * @param isPublic Whether the resource should be available publicly.
     */
    FilesystemResource(IOContext &ioc, std::filesystem::path path, CacheKind cacheKind = CacheKind::fixed,
                       bool isPublic = false) :
        FilesystemResource(ioc, path, {}, cacheKind, isPublic)
    {
    }

    Awaitable<void> operator()(Response &response, Request &request) override;

    bool getAllowGet() const noexcept override;
    bool getAllowNonEmptyPath() const noexcept override;

private:
    IOContext &ioc;
    std::filesystem::path path;
    std::filesystem::path index;
    CacheKind cacheKind;
};

} // namespace Server
