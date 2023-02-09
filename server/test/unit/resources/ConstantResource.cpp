#include "resources/ConstantResource.hpp"

#include "coro_test.hpp"
#include "TestResource.hpp"

namespace
{

CORO_TEST(ConstantResource, Simple, ioc)
{
    Server::ConstantResource resource("Speed of light :)");
    TestRequest request;
    co_await testResource(resource, request, "Speed of light :)");
}

} // namespace
