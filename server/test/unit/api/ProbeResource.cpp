#include "api/ProbeResource.hpp"

#include "coro_test.hpp"
#include "data.hpp"
#include "resources/TestResource.hpp"

namespace
{

CORO_TEST(ApiProbeResource, Simple, ioc)
{
    Api::ProbeResource resource(ioc);
    TestRequest request(Server::Request::Type::post,
                        "[{\"url\":\"" + getSmpteDataPath(1920, 1080, 25, 1, 48000).string() + "\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}}]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, Multiple, ioc)
{
    Api::ProbeResource resource(ioc);
    TestRequest request(Server::Request::Type::post,
                        "[{\"url\":\"" + getSmpteDataPath(1920, 1080, 25, 1, 48000).string() + "\"}," +
                         "{\"url\":\"" + getSmpteDataPath(1920, 1080, 30000, 1001, 48000).string() + "\"}," +
                         "{\"url\":\"" + getSmpteDataPath(1920, 1080, 50, 1, 48000).string() + "\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}},"
                                              "{\"audio\":{\"sampleRate\":48000},"
                                               "\"video\":{\"frameRate\":[30000,1001],\"height\":1080,\"width\":1920}},"
                                              "{\"audio\":{\"sampleRate\":48000},"
                                               "\"video\":{\"frameRate\":[50,1],\"height\":1080,\"width\":1920}}]",
                          "application/json");
}

} // namespace
