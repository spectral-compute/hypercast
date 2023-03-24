#include "resources/ErrorResource.hpp"

#include "coro_test.hpp"
#include "TestResource.hpp"

namespace
{

CORO_TEST(ErrorResource, NotFound, ioc)
{
    Server::ErrorResource resource(Server::ErrorKind::NotFound, Server::CacheKind::ephemeral, true);
    TestRequest request(Server::Request::Type::get, "", true);
    co_await testResourceError(resource, request, Server::ErrorKind::NotFound, Server::CacheKind::ephemeral);
}

} // namespace
