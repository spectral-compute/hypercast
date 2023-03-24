#include "resources/PutResource.hpp"

#include "coro_test.hpp"
#include "TestResource.hpp"

namespace
{

CORO_TEST(PutResource, Simple, ioc)
{
    Server::PutResource resource(Server::CacheKind::fixed, true);

    {
        TestRequest request(Server::Request::Type::put, "Electron", false);
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }

    {
        TestRequest request(Server::Request::Type::get, std::span<const std::span<const std::byte>>(), true);
        co_await testResource(resource, request, "Electron");
    }
}

CORO_TEST(PutResource, Ephemeral, ioc)
{
    Server::PutResource resource(Server::CacheKind::ephemeral, true);

    {
        TestRequest request(Server::Request::Type::put, "Electron", false);
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request(Server::Request::Type::get, std::span<const std::span<const std::byte>>(), true);
        co_await testResource(resource, request, "Electron", {}, Server::CacheKind::ephemeral);
    }
}

CORO_TEST(PutResource, NotFound, ioc)
{
    Server::PutResource resource;
    TestRequest request(Server::Request::Type::get, "", true);
    co_await testResourceError(resource, request, "PUT resource was GET'd before being PUT", Server::ErrorKind::NotFound);
}

CORO_TEST(PutResource, Rewrite, ioc)
{
    Server::PutResource resource(Server::CacheKind::fixed, true);

    {
        TestRequest request(Server::Request::Type::put, "Electron", false);
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request(Server::Request::Type::get, std::span<const std::span<const std::byte>>(), true);
        co_await testResource(resource, request, "Electron");
    }
    {
        TestRequest request(Server::Request::Type::put, "Muon", false);
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request(Server::Request::Type::get, std::span<const std::span<const std::byte>>(), true);
        co_await testResource(resource, request, "Muon");
    }
}

} // namespace
