#pragma once

#include "server/CacheKind.hpp"
#include "server/SynchronousResource.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Server
{

/**
 * A file-like resource whose content is fixed until it's removed or replaced.
 */
class ConstantResource final : public SynchronousNullaryResource
{
public:
    ~ConstantResource() override;

    /**
     * Construct a resource with constant content.
     *
     * @param content The content to return when this resource is requested.
     * @param mimeType The MIME type for the content.
     * @param cacheKind The caching to use for the resource.
     * @param isPublic Whether the resource should be available publicly.
     */
    ConstantResource(std::vector<std::byte> content, std::string mimeType = "",
                     CacheKind cacheKind = CacheKind::fixed, bool isPublic = false) :
        SynchronousNullaryResource(isPublic),
        content(std::move(content)), mimeType(std::move(mimeType)), cacheKind(cacheKind)
    {
    }

    /**
     * @copydoc ConstantResource
     */
    ConstantResource(std::span<const std::byte> content, std::string mimeType = "",
                     CacheKind cacheKind = CacheKind::fixed, bool isPublic = false) :
        ConstantResource(std::vector<std::byte>(content.begin(), content.end()), std::move(mimeType),
                         cacheKind, isPublic)
    {
    }

    /**
     * @copydoc ConstantResource
     */
    ConstantResource(std::string_view content, std::string mimeType = "", CacheKind cacheKind = CacheKind::fixed,
                     bool isPublic = false) :
        ConstantResource(std::span((const std::byte *)content.data(), content.size()), std::move(mimeType),
                         cacheKind, isPublic)
    {
    }

    void getSync(Response &response, const Request &request) override;

private:
    std::vector<std::byte> content;
    std::string mimeType;
    CacheKind cacheKind;
};

} // namespace Server
