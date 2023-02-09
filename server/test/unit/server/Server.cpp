#include "TestServer.hpp"

namespace
{

SERVER_TEST(Server, Simple, server)
{
    server.addResource("alpha/beta");
    co_await server("alpha/beta");
}

SERVER_TEST(Server, NotFound, server)
{
    co_await server("alpha/beta", Server::ErrorKind::NotFound);
}

SERVER_TEST(Server, Error, server)
{
    server.addResource("alpha/beta", nullptr);
    co_await server("alpha/beta", 0, false, ::Server::Request::Type::get, {}, true);
}

SERVER_TEST(Server, Removed, server)
{
    server.addResource("alpha/beta");
    server.removeResource("alpha/beta");
    co_await server("alpha/beta", Server::ErrorKind::NotFound);
}

SERVER_TEST(Server, RemoveOneOfTwo, server)
{
    server.addResource("alpha");
    server.addResource("beta");
    server.removeResource("alpha");
    co_await server("alpha", Server::ErrorKind::NotFound);
    co_await server("beta", 1);
}

SERVER_TEST(Server, Recreated, server)
{
    server.addResource("alpha/beta");
    server.removeResource("alpha/beta");
    server.addResource("alpha/beta");
    co_await server("alpha/beta", 1);
}

SERVER_TEST(Server, Replaced, server)
{
    server.addResource("alpha/beta");
    server.addOrReplaceResource("alpha/beta");
    co_await server("alpha/beta", 1);
}

SERVER_TEST(Server, DenyPublic, server)
{
    server.addResource("alpha/beta");
    co_await server("alpha/beta", Server::ErrorKind::Forbidden, true);
}

SERVER_TEST(Server, AllowPrivate, server)
{
    server.addResource("alpha/beta");
    co_await server("alpha/beta");
}

SERVER_TEST(Server, Two, server)
{
    server.addResource("alpha");
    server.addResource("beta");
    co_await server("alpha");
    co_await server("beta", 1);
}

SERVER_TEST(Server, UnsupportedType, server)
{
    server.addResource("alpha/beta");
    co_await server("alpha/beta", Server::ErrorKind::UnsupportedType, false, ::Server::Request::Type::post);
}

SERVER_TEST(Server, SubPath, server)
{
    server.addResource("alpha");
    co_await server("alpha/beta", 0, false, ::Server::Request::Type::get, "beta");
}

SERVER_TEST(Server, BadSubPath, server)
{
    server.addResource("alpha", false, false);
    co_await server("alpha/beta", Server::ErrorKind::NotFound);
}

SERVER_TEST(Server, RemoveNonexistent, server)
{
    EXPECT_THROW(server.removeResource("alpha"), std::runtime_error);
    co_await server("alpha", Server::ErrorKind::NotFound);
}

SERVER_TEST(Server, RemoveIntermediate, server)
{
    server.addResource("alpha/beta");
    EXPECT_THROW(server.removeResource("alpha"), std::runtime_error);
    co_await server("alpha/beta");
}

SERVER_TEST(Server, RemoveChildOfLeaf, server)
{
    server.addResource("alpha");
    EXPECT_THROW(server.removeResource("alpha/beta"), std::runtime_error);
    co_await server("alpha");
    co_await server("alpha/beta", 0, false, ::Server::Request::Type::get, "beta"); // Because the leaf supports it.
}

SERVER_TEST(Server, AllowPublicGet, server)
{
    /* GET is OK publicly or privately. */
    server.addResource("alpha/beta", true, false, true, true, false);
    co_await server("alpha/beta", 0, true);
}

SERVER_TEST(Server, AllowPrivateGet, server)
{
    /* GET is OK publicly or privately. */
    server.addResource("alpha/beta", true, false, true, true, false);
    co_await server("alpha/beta", 0, false);
}

SERVER_TEST(Server, DenyPublicPost, server)
{
    /* POST is only OK privately. */
    server.addResource("alpha/beta", true, false, true, true, false);
    co_await server("alpha/beta", Server::ErrorKind::Forbidden, true, Server::Request::Type::post);
}

SERVER_TEST(Server, AllowPrivatePost, server)
{
    /* POST is only OK privately. */
    server.addResource("alpha/beta", true, false, true, true, false);
    co_await server("alpha/beta", 0, false, Server::Request::Type::post);
}

} // namespace
