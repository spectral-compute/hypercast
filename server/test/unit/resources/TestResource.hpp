#pragma once

#include "server/CacheKind.hpp"
#include "server/Error.hpp"
#include "server/Request.hpp"

#include "util/asio.hpp"

#include <span>

namespace Server
{

class Resource;

} // namespace Server

/**
 * A test implementation of Server::Request.
 *
 * Handily, that class is abstract to hide away the complexity of interfacing with the HTTP server, so we can just do
 * this.
 */
class TestRequest final : public Server::Request
{
public:
    ~TestRequest() override;

    /**
     * Constructor :)
     *
     * @param path The path of the request.
     * @param type The request type.
     * @param data The data to send to the resource. For 2D versions of this, the data is sent one piece at a time,
     *             according to the given pieces.
     * @param isPublic Whether to treat the request as though it came from the public Internet.
     * @param expectPartialRead Whether to expect that the full request body won't be read (as can be the case when an
     *                          error is expected to occur).
     */
    TestRequest(Server::Path path, Type type = Server::Request::Type::get,
                std::span<const std::span<const std::byte>> data = {},
                bool isPublic = false, bool expectPartialRead = false);

    TestRequest(Server::Path path, Type type, std::span<const std::byte> data, bool isPublic = false,
                bool expectPartialRead = false) :
        TestRequest(std::move(path), type, std::span<const std::span<const std::byte>>({data}), isPublic,
                    expectPartialRead)
    {
    }

    TestRequest(Server::Path path, Type type, std::string_view string, bool isPublic = false,
                bool expectPartialRead = false) :
        TestRequest(std::move(path), type, std::span((const std::byte *)string.data(), string.size()), isPublic,
                    expectPartialRead)
    {
    }

    TestRequest(Type type = Server::Request::Type::get, std::span<const std::span<const std::byte>> data = {},
                bool isPublic = false,  bool expectPartialRead = false) :
        TestRequest("", type, data, isPublic, expectPartialRead)
    {
    }

    TestRequest(Type type, std::span<const std::byte> data, bool isPublic = false,
                bool expectPartialRead = false) :
        TestRequest("", type, std::span<const std::span<const std::byte>>({data}), isPublic, expectPartialRead)
    {
    }

    TestRequest(Type type, std::string_view string, bool isPublic = false,
                bool expectPartialRead = false) :
        TestRequest("", type, std::span((const std::byte *)string.data(), string.size()), isPublic, expectPartialRead)
    {
    }

    Awaitable<std::vector<std::byte>> readSome() override;

private:
    std::vector<std::vector<std::byte>> data;
    size_t dataReadIndex = 0;
    bool fullyRead = false;
    bool expectPartialRead = false;
};

/**
 * Check that the response to a request of a resource is as expected.
 *
 * @param request The request to pass ot the resource.
 * @param resource The resource to make a request of.
 * @param result The data that should have been written, split into the chunks that should have been written.
 * @param mimeType The expected MIME type.
 * @param cacheKind The expected cache.
 * @param errorKind The expected error kind.
 */
Awaitable<void> testResource(Server::Resource &resource,
                             TestRequest &request,
                             std::span<const std::span<const std::byte>> result,
                             std::string_view mimeType = {},
                             Server::CacheKind cacheKind = Server::CacheKind::fixed,
                             std::optional<Server::ErrorKind> errorKind = {});

/**
 * Check that the response to a request of a resource is as expected.
 *
 * @param request The request to pass ot the resource.
 * @param resource The resource to make a request of.
 * @param result The data that should have been written.
 * @param mimeType The expected MIME type.
 * @param cacheKind The expected cache.
 * @param errorKind The expected error kind.
 */
Awaitable<void> testResource(Server::Resource &resource,
                             TestRequest &request,
                             std::span<const std::byte> result,
                             std::string_view mimeType = {},
                             Server::CacheKind cacheKind = Server::CacheKind::fixed,
                             std::optional<Server::ErrorKind> errorKind = {});

/**
 * @copydoc testServerResource
 */
inline Awaitable<void> testResource(Server::Resource &resource,
                                    TestRequest &request,
                                    std::string_view result,
                                    std::string_view mimeType = {},
                                    Server::CacheKind cacheKind = Server::CacheKind::fixed,
                                    std::optional<Server::ErrorKind> errorKind = {})
{
    return testResource(resource, request, {(const std::byte *)result.data(), result.size()}, mimeType, cacheKind,
                        errorKind);
}

/**
 * @copydoc testServerResource
 */
inline Awaitable<void> testResource(Server::Resource &resource,
                                    TestRequest &request,
                                    Server::CacheKind cacheKind = Server::CacheKind::fixed,
                                    std::optional<Server::ErrorKind> errorKind = {})
{
    return testResource(resource, request, std::span<const std::byte>{}, {}, cacheKind, errorKind);
}

/**
 * Check that the response to a request of a resource is a given error.
 *
 * Since almost all errors are strings (or unset) and have a MIME type that is either text/plain (if the message is
 * present) or unset (otherwise), this method calculates the expected MIME type.
 *
 * @param resource The resource to make a request of.
 * @param request The request to pass ot the resource.
 * @param message The expected message of the error. Some variants don't have this.
 * @param errorKind The expected error kind.
 * @param cacheKind The expected cache kind.
 */
inline Awaitable<void> testResourceError(Server::Resource &resource,
                                         TestRequest &request,
                                         std::string_view message,
                                         Server::ErrorKind errorKind,
                                         Server::CacheKind cacheKind = Server::CacheKind::fixed)
{
    return testResource(resource, request, message, message.empty() ? std::string_view{} : "text/plain", cacheKind,
                        errorKind);
}

/**
 * @copydoc testServerResourceError
 */
inline Awaitable<void> testResourceError(Server::Resource &resource,
                                         TestRequest &request,
                                         Server::ErrorKind errorKind,
                                         Server::CacheKind cacheKind = Server::CacheKind::fixed)
{
    return testResource(resource, request, cacheKind, errorKind);
}
