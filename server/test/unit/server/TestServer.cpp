#include "TestServer.hpp"

#include "server/Response.hpp"
#include "server/SynchronousResource.hpp"

#include "util/asio.hpp"

#include <gtest/gtest.h>

namespace
{

/**
 * A "record" object that can pass through the server's response body mechanism.
 *
 * The record describes what resource was requested (@see getResourceIndex), what sub-path was given to that resource
 * (@see getPath), and what the outcome of trying to handle it was (@see getType).
 */
class ServerTestRecord final
{
public:
    static constexpr int defaultType = -1; ///< Default construction. This means no record was received.
    static constexpr int errorType = -2; ///< The test resource threw a Server::Error.
    static constexpr int undecodableType = -3; ///< The record could not be decoded at all.

    ServerTestRecord() = default;

    /**
     * Constructor for "resource threw an error".
     *
     * @param testResourceIndex The resource index for the error.
     */
    ServerTestRecord(int testResourceIndex) : header({ .type = errorType, .testResourceIndex = testResourceIndex })
    {
    }

    /**
     * Construct from a request type and resource index.
     *
     * @param type The request type.
     * @param testResourceIndex The resource index.
     * @param path The path that should have been given to the resource.
     */
    ServerTestRecord(Server::Request::Type type, int testResourceIndex, std::string path = "") :
        header({ .type = (int)type, .testResourceIndex = testResourceIndex }),
        path(std::move(path))
    {
    }

    /**
     * Construct from bytes.
     *
     * This is so we can reconstruct one of these objects after it's travelled through the response body mechanism.
     */
    ServerTestRecord(std::span<const std::byte> bytes)
    {
        /* Decode the header. */
        if (bytes.size() < sizeof(header)) {
            ADD_FAILURE() << "Record data is too small size: " << bytes.size();
            header.type = undecodableType;
            return;
        }
        memcpy(&header, bytes.data(), sizeof(header));

        /* Decode the path. */
        path.resize(bytes.size() - sizeof(header));
        memcpy(path.data(), bytes.data() + sizeof(header), path.size());
    }

    /**
     * Convert tp bytes.
     *
     * This is so this object can travel through the response body mechanism.
     */
    operator std::vector<std::byte>() const
    {
        std::vector<std::byte> bytes(sizeof(header) + path.size());
        memcpy(bytes.data(), &header, sizeof(header));
        memcpy(bytes.data() + sizeof(header), path.data(), path.size());
        return bytes;
    }

    /**
     * Convert tp bytes.
     *
     * This is so this object can travel through the request error handling mechanism.
     */
    operator std::string() const
    {
        std::vector<std::byte> bytes = *this;
        return std::string((const char *)bytes.data(), bytes.size());
    }

    /**
     * Get the request type.
     *
     * This is either a Server::Request::Type reflecting the type of request the resource received, or one of the
     * special values for this class.
     */
    int getType() const
    {
        return header.type;
    }

    /**
     * Get the resource index for the resource that serviced the request.
     *
     * Every time a resource is created, it gets a unique index. That way, we can track resources with the same path but
     * that have been deleted and re-created. Resource indices start from 0.
     *
     * A negative resource index indicates no resource successfully responded.
     */
    int getResourceIndex() const
    {
        return header.testResourceIndex;
    }

    /**
     * The path given to the resource.
     *
     * This only get set for non-error responses.
     */
    const std::string &getPath() const
    {
        return path;
    }

private:
    struct
    {
        int type = defaultType;
        int testResourceIndex = -1;
    } header;
    std::string path;
};

/**
 * A test resource that sends ServerTestRecord objects via the response body mechanism.
 */
class ServerTestResource final : public Server::SynchronousNullaryResource
{
public:
    ~ServerTestResource() override = default;

    /**
     * Create a resource with a given set of permissions and capabilities.
     *
     * Setting these permissions and capabilities is useful to test the server's enforcement of the restrictions.
     */
    ServerTestResource(int testResourceIndex, bool isPublic, bool allowNonEmptyPath, bool allowGet, bool allowPost,
                       bool allowPut) :
        SynchronousNullaryResource(isPublic),
        testResourceIndex(testResourceIndex), isAlwaysError(false),
        allowNonEmptyPath(allowNonEmptyPath), allowGet(allowGet), allowPost(allowPost), allowPut(allowPut)
    {
    }

    /**
     * Construct a resource that always throws an error.
     *
     * The particular error that's thrown is one that decodes to ServerTestRecord::errorType. Errors that come from the
     * server don't write bodies, and therefore will have a record whose type decodes to ServerTestRecord::defaultType.
     */
    ServerTestResource(int testResourceIndex) :
        SynchronousNullaryResource(false), testResourceIndex(testResourceIndex)
    {
    }

    void operator()(Server::Response &response, const Server::Request &request) override
    {
        if (isAlwaysError) {
            throw Server::Error(Server::ErrorKind::Internal, ServerTestRecord(testResourceIndex));
        }
        response << ServerTestRecord(request.getType(), testResourceIndex, request.getPath());
    }

    bool getAllowNonEmptyPath() const noexcept override { return allowNonEmptyPath; }
    bool getAllowGet() const noexcept override { return allowGet; }
    bool getAllowPost() const noexcept override { return allowPost; }
    bool getAllowPut() const noexcept override { return allowPut; }

private:
    const int testResourceIndex;
    const bool isAlwaysError = true;

    const bool allowNonEmptyPath = true;
    const bool allowGet = true;
    const bool allowPost = true;
    const bool allowPut = true;
};

/**
 * A very simple implementation of Server::Request that never has any data to read.
 */
class ServerTestRequest final : public Server::Request
{
public:
    ~ServerTestRequest() override = default;
    using Request::Request;

    Awaitable<std::vector<std::byte>> readSome() override
    {
        co_return std::vector<std::byte>{};
    }
};

/**
 * A test response object that decodes what it receives to ServerTestRecord.
 */
class ServerTestResponse final : public Server::Response
{
public:
    ~ServerTestResponse() override = default;

    /**
     * Check that the response is as expected.
     */
    void check(const ServerTestRecord &refRecord, bool expectedRecord,
               std::optional<::Server::ErrorKind> errorKind) const
    {
        EXPECT_TRUE(ended);
        EXPECT_TRUE(awaitedAllWrites);
        EXPECT_TRUE(getWriteStarted());
        EXPECT_EQ(expectedRecord, written);
        EXPECT_EQ(errorKind, getErrorKind()) << "Expected: " << formatErrorKind(errorKind)
                                             << ", actual: " << formatErrorKind(getErrorKind());
        EXPECT_EQ(refRecord.getType() == ServerTestRecord::errorType ? "text/plain" : "",
                  getMimeType()); // Non-empty error messages always have text MIME type.
        EXPECT_EQ(refRecord.getType(), record.getType());
        EXPECT_EQ(refRecord.getResourceIndex(), record.getResourceIndex());
        EXPECT_EQ(refRecord.getPath(), record.getPath());
    }

private:
    /**
     * Decode a record.
     */
    void writeBody(std::vector<std::byte> data) override
    {
        EXPECT_FALSE(ended);
        EXPECT_FALSE(written);
        EXPECT_FALSE(getWriteStarted());
        record = ServerTestRecord(std::move(data));
        if (record.getType() == ServerTestRecord::defaultType) {
            ADD_FAILURE() << "Default record type was received as response body.";
        }
        written = true;
        awaitedAllWrites = false;
    }

    Awaitable<void> flushBody(bool end) override
    {
        EXPECT_FALSE(ended);
        if (end) {
            ended = true;
        }
        awaitedAllWrites = true;
        co_return;
    }

    /**
     * Format an optional error kind as a human readable string.
     */
    static std::string formatErrorKind(const std::optional<Server::ErrorKind> errorKind)
    {
        if (!errorKind) {
            return "None";
        }
        switch (*errorKind) {
            case Server::ErrorKind::BadRequest: return "Bad request";
            case Server::ErrorKind::Forbidden: return "Forbidden";
            case Server::ErrorKind::NotFound: return "Not found";
            case Server::ErrorKind::UnsupportedType: return "Unsupported type";
            case Server::ErrorKind::Internal: return "Internal";
        }
        return "Unknown: " + std::to_string((int)*errorKind);
    }

    ServerTestRecord record;
    bool written = false;
    bool ended = false;
    bool awaitedAllWrites = true;
};

} // namespace

TestServer::~TestServer() = default;

void TestServer::addResource(const ::Server::Path &path, bool isPublic, bool allowNonEmptyPath,
                             bool allowGet, bool allowPost, bool allowPut)
{
    Server::addResource<ServerTestResource>(path, testResourceNextIndex++, isPublic, allowNonEmptyPath,
                                            allowGet, allowPost, allowPut);
}

void TestServer::addResource(const ::Server::Path &path, std::nullptr_t)
{
    Server::addResource<ServerTestResource>(path, testResourceNextIndex++);
}

void TestServer::addOrReplaceResource(const ::Server::Path &path, bool isPublic, bool allowNonEmptyPath,
                                      bool allowGet, bool allowPost, bool allowPut)
{
    Server::addOrReplaceResource<ServerTestResource>(path, testResourceNextIndex++, isPublic, allowNonEmptyPath,
                                                     allowGet, allowPost, allowPut);
}

void TestServer::addOrReplaceResource(const ::Server::Path &path, std::nullptr_t)
{
    Server::addOrReplaceResource<ServerTestResource>(path, testResourceNextIndex++);
}

Awaitable<void> TestServer::operator()(::Server::Path path, int expectedResourceIndex, bool isPublic,
                                       ::Server::Request::Type type, std::string_view expectedPath, bool expectedError)
{
    ServerTestRequest request(std::move(path), type, isPublic);
    ServerTestResponse response;
    co_await Server::operator()(response, request);
    response.check(expectedError ? expectedResourceIndex :
                                   ServerTestRecord(type, expectedResourceIndex, std::string(expectedPath)),
                   true, expectedError ? std::optional(::Server::ErrorKind::Internal) : std::nullopt);
}

Awaitable<void> TestServer::operator()(::Server::Path path, ::Server::ErrorKind errorKind, bool isPublic,
                                       ::Server::Request::Type type)
{
    ServerTestRequest request(path, type, isPublic);
    ServerTestResponse response;
    co_await Server::operator()(response, request);
    response.check({}, false, errorKind);
}
