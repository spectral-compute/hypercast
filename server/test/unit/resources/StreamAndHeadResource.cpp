#include "resources/StreamAndHeadResource.hpp"

#include "coro_test.hpp"
#include "TestResource.hpp"

namespace
{

/* Check the basic functionality, */
CORO_TEST(StreamAndHeadResource, Simple, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    {
        TestRequest request("stream", Server::Request::Type::put, "Electrons are fundamental particles");
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("head");
        co_await testResource(resource, request, "Electrons are fun", {}, Server::CacheKind::none);
    }
    {
        TestRequest request("stream");
        co_await testResource(resource, request, "Electrons are fundamental particles", {}, Server::CacheKind::none);
    }
}

/* Check that nothing goes wrong if the stream is shorter than the maximum head size. */
CORO_TEST(StreamAndHeadResource, Short, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    {
        TestRequest request("stream", Server::Request::Type::put, "Electrons");
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("head");
        co_await testResource(resource, request, "Electrons", {}, Server::CacheKind::none);
    }
    {
        TestRequest request("stream");
        co_await testResource(resource, request, "Electrons", {}, Server::CacheKind::none);
    }
}

/* Check that receiving the data in chunks doesn't cause corruption. */
CORO_TEST(StreamAndHeadResource, Words, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    const std::string_view string = "Electrons are fundamental particles";
    const std::span<const std::byte> data((const std::byte *)string.data(), string.size());
    const std::span<const std::byte> parts[] = {
        data.subspan(0, 10),
        data.subspan(10, 14),
        data.subspan(14, 26),
        data.subspan(26),
    };

    {
        TestRequest request("stream", Server::Request::Type::put, parts);
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("head");
        co_await testResource(resource, request, "Electrons are fun", {}, Server::CacheKind::none);
    }
    {
        TestRequest request("stream");
        co_await testResource(resource, request, parts, {}, Server::CacheKind::none);
    }
}

/* Check that we can read the head multiple times. */
CORO_TEST(StreamAndHeadResource, DoubleGetHead, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    {
        TestRequest request("stream", Server::Request::Type::put, "Electrons are fundamental particles");
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("head");
        co_await testResource(resource, request, "Electrons are fun", {}, Server::CacheKind::none);
    }
    {
        TestRequest request("stream");
        co_await testResource(resource, request, "Electrons are fundamental particles", {}, Server::CacheKind::none);
    }
    {
        TestRequest request("head");
        co_await testResource(resource, request, "Electrons are fun", {}, Server::CacheKind::none);
    }
}

/* Test that if the single-write input is longer than the buffer, we don't deadlock. */
CORO_TEST(StreamAndHeadResource, Long, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 4, "head", 17);

    {
        TestRequest request("stream", Server::Request::Type::put, "Electrons are fundamental particles");
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("head");
        co_await testResource(resource, request, "Electrons are fun", {}, Server::CacheKind::none);
    }
    {
        TestRequest request("stream");
        co_await testResource(resource, request, "Electrons are fundamental particles", {}, Server::CacheKind::none);
    }
}

/* Check that we can't PUT to the head. */
CORO_TEST(StreamAndHeadResource, PutHead, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    {
        TestRequest request("head", Server::Request::Type::put, "Electrons are fundamental particles");
        co_await testResourceError(resource, request, "Cannot put the stream head", Server::ErrorKind::UnsupportedType,
                                   Server::CacheKind::none);
    }
}

/* Check that we 404 unknown paths. */
CORO_TEST(StreamAndHeadResource, GetUnknown, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    {
        TestRequest request("stream", Server::Request::Type::put, "Electrons are fundamental particles");
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("foot");
        co_await testResourceError(resource, request, "Neither stream nor head requested", Server::ErrorKind::NotFound,
                                   Server::CacheKind::none);
    }
}

/* Check that only one client can PUT the stream. */
CORO_TEST(StreamAndHeadResource, DoublePut, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    {
        TestRequest request("stream", Server::Request::Type::put, "Electrons are fundamental particles");
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("stream", Server::Request::Type::put, "So are muons");
        co_await testResourceError(resource, request, "Client already connected", Server::ErrorKind::Conflict,
                                   Server::CacheKind::none);
    }
}

/* Check that only one client can GET the stream. */
CORO_TEST(StreamAndHeadResource, DoubleGet, ioc)
{
    Server::StreamAndHeadResource resource(ioc, "stream", 1 << 20, "head", 17);

    {
        TestRequest request("stream", Server::Request::Type::put, "Electrons are fundamental particles");
        co_await testResource(resource, request, std::span<const std::span<const std::byte>>{}, {},
                              Server::CacheKind::none);
    }
    {
        TestRequest request("stream");
        co_await testResource(resource, request, "Electrons are fundamental particles", {}, Server::CacheKind::none);
    }
    {
        TestRequest request("stream");
        co_await testResourceError(resource, request, "Client already connected", Server::ErrorKind::Conflict,
                                   Server::CacheKind::none);
    }
}

} // namespace
