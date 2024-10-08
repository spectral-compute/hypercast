#include "TestResource.hpp"

#include "server/Resource.hpp"
#include "server/Response.hpp"

#include <gtest/gtest.h>

#include <cassert>
#include <string_view>
#include <vector>

namespace
{

std::string cacheKindToString(std::optional<Server::CacheKind> cacheKind)
{
    if (!cacheKind) {
        return "Null";
    }
    switch (*cacheKind) {
        case Server::CacheKind::none: return "None";
        case Server::CacheKind::ephemeral: return "Ephemeral";
        case Server::CacheKind::fixed: return "Fixed";
        case Server::CacheKind::indefinite: return "Indefinite";
    }
    return "Unknown";
}

std::string errorKindToString(std::optional<Server::ErrorKind> errorKind)
{
    if (!errorKind) {
        return "Null";
    }
    switch (*errorKind) {
        case Server::ErrorKind::BadRequest: return "Bad request";
        case Server::ErrorKind::Forbidden: return "Forbidden";
        case Server::ErrorKind::NotFound: return "Not found";
        case Server::ErrorKind::UnsupportedType: return "Unsupported type";
        case Server::ErrorKind::Conflict: return "Conflict";
        case Server::ErrorKind::Internal: return "Internal";
    }
    return "Unknown";
}

/**
 * Tell if some data is actually text.
 */
bool isText(std::span<const std::byte> data)
{
    for (std::byte b: data) {
        char c = (char)b;
        if (!std::isprint(c) && !std::isspace(c)) {
            return false;
        }
    }
    return true;
}

/**
 * A test implementation of Server::Response.
 *
 * Handily, that class is abstract to hide away the complexity of interfacing with the HTTP server, so we can just do
 * this.
 */
class TestResponse final : public Server::Response
{
public:
    /**
     * Check that the response is as expected.
     *
     * @param checkChunks Whether it should be checked that the result was split into chunks exactly as given in data.
     * @param data The data that should have been written.
     * @param mimeType The expected MIME type.
     * @param cacheKind The expected cache.
     * @param errorKind The expected error kind.
     */
    void check(bool checkChunks, std::span<const std::span<const std::byte>> data, std::string_view mimeType,
               Server::CacheKind cacheKind, std::optional<Server::ErrorKind> errorKind) const
    {
        std::vector<std::byte> accumulatedRef;
        for (const auto &chunk: data) {
            accumulatedRef.insert(accumulatedRef.end(), chunk.begin(), chunk.end());
        }

        EXPECT_FALSE(ended); // This should only be set by the server.
        EXPECT_EQ(writeStarted, getWriteStarted());
        EXPECT_EQ(errorKind, getErrorKind()) << "Reference error kind: " << errorKindToString(errorKind)
                                             << ", actual error kind: " << errorKindToString(getErrorKind());
        EXPECT_EQ(cacheKind, getCacheKind()) << "Reference cache kind: " << cacheKindToString(cacheKind)
                                             << ", actual cache kind: " << cacheKindToString(getCacheKind());
        EXPECT_EQ(mimeType, getMimeType());
        EXPECT_EQ(accumulatedRef.size(), accumulatedData.size()); // Easier to read than the next one.

        if (isText(accumulatedRef)) {
            EXPECT_EQ(accumulatedRef, accumulatedData)
                << "Reference data: " << std::string((const char *)accumulatedRef.data(), accumulatedRef.size())
                << ", actual data: " << std::string((const char *)accumulatedData.data(), accumulatedData.size());
        }
        else {
            EXPECT_EQ(accumulatedRef, accumulatedData);
        }

        if (checkChunks) {
            EXPECT_EQ(data.size(), chunkedData.size());
            for (size_t i = 0; i < std::min(data.size(), chunkedData.size()); i++) {
                EXPECT_EQ(std::vector(data[i].begin(), data[i].end()), chunkedData[i]) << "Chunk index: " << i;
            }
        }
    }

private:
    void writeBody(std::vector<std::byte> data) override
    {
        EXPECT_FALSE(ended);
        EXPECT_EQ(writeStarted, getWriteStarted());
        accumulatedData.insert(accumulatedData.end(), data.begin(), data.end());
        chunkedData.emplace_back(std::move(data));
        writeStarted = true;
    }

    Awaitable<void> flushBody(bool end) override
    {
        EXPECT_FALSE(ended);
        if (end) {
            ended = true;
        }
        co_return;
    }

    std::vector<std::vector<std::byte>> chunkedData;
    std::vector<std::byte> accumulatedData;
    bool writeStarted = false;
    bool ended = false;
};

Awaitable<void> testResourceImpl(Server::Resource &resource, TestRequest &request, bool checkChunks,
                                 std::span<const std::span<const std::byte>> result, std::string_view mimeType,
                                 Server::CacheKind cacheKind, std::optional<Server::ErrorKind> errorKind)
{
    /* Check that the test is valid. */
    assert(request.getPath().empty() || resource.getAllowNonEmptyPath());

    /* Perform the test itself. */
    TestResponse response;
    try {
        co_await resource(response, request);
    }
    catch (const Server::Error &e) {
        if (response.getWriteStarted()) {
            ADD_FAILURE() << "Error exception thrown after writing started. Kind: " << std::to_string((int)e.kind)
                          << ", message: \"" << e.message << "\".";
        }
        else {
            response.setErrorAndMessage(e.kind, e.message);
        }
    }
    catch (const std::exception &e) {
        ADD_FAILURE() << "Exception from Resource::operator(): " << e.what();
        throw;
    }
    catch (...) {
        ADD_FAILURE() << "Exception from Resource::operator().";
        throw;
    }

    /* Check the result. */
    response.check(checkChunks, result, mimeType, cacheKind, errorKind);
}

} // namespace

TestRequest::TestRequest(Server::Path path, Type type, std::span<const std::span<const std::byte>> data, bool isPublic) :
    Request(std::move(path), type, isPublic), data(data.size())
{
    int acc = 0;
    for (size_t i = 0; i < data.size(); i++) {
        this->data[i].insert(this->data[i].end(), data[i].begin(), data[i].end());
        acc += (int) this->data[i].size();
    }
    this->setMaxLength(acc);
}

Awaitable<std::vector<std::byte>> TestRequest::doReadSome()
{
    std::vector<std::byte> result;
    if (dataReadIndex < data.size()) {
        result = std::move(data[dataReadIndex++]);
    }
    else {
        // This isn't even necessarily a bug, since the implementation will return empty-vector every time (and it will
        // only return empty-vector when the end has been reached). It won't hang. It would certainly be slightly odd,
        // but a Resource could perfectly legitimately hit the end repeatedly.
        EXPECT_FALSE(fullyRead) << "Attempt to read request body after end of request body empty data returned.";
        fullyRead = true;
    }
    co_return result;
}

Awaitable<void> testResource(Server::Resource &resource, TestRequest &request,
                             std::span<const std::span<const std::byte>> result, std::string_view mimeType,
                             Server::CacheKind cacheKind, std::optional<Server::ErrorKind> errorKind)
{
    return testResourceImpl(resource, request, true, result, mimeType, cacheKind, errorKind);
}

Awaitable<void> testResource(Server::Resource &resource, TestRequest &request, std::span<const std::byte> result,
                             std::string_view mimeType, Server::CacheKind cacheKind,
                             std::optional<Server::ErrorKind> errorKind)
{
    co_return co_await testResourceImpl(resource, request, false, {{result}}, mimeType, cacheKind, errorKind);
}
