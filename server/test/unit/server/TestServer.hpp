#pragma once

#include "server/Server.hpp"

#include "server/Error.hpp"
#include "server/Request.hpp"

#include "coro_test.hpp"
#include "log.hpp"

/**
 * A test server that submits requests to the base class and checks which resources respond and what errors occur.
 */
class TestServer final : private Server::Server
{
public:
    ~TestServer() override;
    TestServer(Log::Log &log) : Server(log) {}

    using Server::removeResource;

    /**
     * Add a resource with a given set of permissions and capabilities.
     *
     * Setting these permissions and capabilities is useful to test the server's enforcement of the restrictions.
     */
    void addResource(const ::Server::Path &path, bool isPublic = false, bool allowNonEmptyPath = true,
                     bool allowGet = true, bool allowPost = false, bool allowPut = false);

    /**
     * Add a resource that always throws an error.
     */
    void addResource(const ::Server::Path &path, std::nullptr_t);

    /**
     * Add a resource with a given set of permissions and capabilities.
     *
     * Setting these permissions and capabilities is useful to test the server's enforcement of the restrictions.
     */
    void addOrReplaceResource(const ::Server::Path &path, bool isPublic = false, bool allowNonEmptyPath = true,
                              bool allowGet = true, bool allowPost = false, bool allowPut = false);

    /**
     * Add a resource that always throws an error.
     */
    void addOrReplaceResource(const ::Server::Path &path, std::nullptr_t);

    /**
     * Try a request, and expect a resource to respond.
     *
     * @param path The path for the request.
     * @param expectedResourceIndex The resource index of the resource that is expected to respond.  Every time a
     *                              resource is added, it gets a unique index starting from 0. This number is the index
     *                              that the resource that should respond would have.
     * @param isPublic Whether the request is public.
     * @param type The type of request.
     * @param expectedPath The path that was expected to be given to the resource that responded.
     * @param expectedError Whether the resource is expected to respond with an error.
     */
    Awaitable<void> operator()(::Server::Path path, int expectedResourceIndex = 0, bool isPublic = false,
                               ::Server::Request::Type type = ::Server::Request::Type::get,
                               std::string_view expectedPath = {}, std::optional<::Server::ErrorKind> expectedError = std::nullopt);

    /**
     * Try a request, and expect an error that does not originate from a resource.
     *
     * @param path The path for the request.
     * @param errorKind The error to expect.
     * @param isPublic Whether the request is public.
     * @param type The type of request.
     */
    Awaitable<void> operator()(::Server::Path path, ::Server::ErrorKind errorKind, bool isPublic = false,
                               ::Server::Request::Type type = ::Server::Request::Type::get);

private:
    int testResourceNextIndex = 0;
};

/**
 * Create a GTest test that the test server.
 */
#define SERVER_TEST(TestSuiteName, TestName, ServerName) \
    static Awaitable<void> SERVER_TEST__##TestSuiteName__##TestName(TestServer &ServerName); \
    CORO_TEST(TestSuiteName, TestName, ioc) \
    { \
        ExpectNeverLog log(ioc); \
        TestServer server(log); \
        co_await SERVER_TEST__##TestSuiteName__##TestName(server); \
    } \
    static Awaitable<void> SERVER_TEST__##TestSuiteName__##TestName(TestServer &ServerName)
