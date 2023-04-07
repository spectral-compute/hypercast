#include "api/ProbeResource.hpp"

#include "ffmpeg/ffprobe.hpp"

#include "coro_test.hpp"
#include "data.hpp"
#include "resources/TestResource.hpp"

namespace
{

CORO_TEST(ApiProbeResource, Simple, ioc)
{
    std::set<std::string> inUseUrls;
    Api::ProbeResource resource(ioc, inUseUrls);

    TestRequest request(Server::Request::Type::post,
                        "[{\"url\":\"" + getSmpteDataPath(1920, 1080, 25, 1, 48000).string() + "\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},\"inUse\":false,"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}}]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, Multiple, ioc)
{
    std::set<std::string> inUseUrls;
    Api::ProbeResource resource(ioc, inUseUrls);

    TestRequest request(Server::Request::Type::post,
                        "[{\"url\":\"" + getSmpteDataPath(1920, 1080, 25, 1, 48000).string() + "\"}," +
                         "{\"url\":\"" + getSmpteDataPath(1920, 1080, 30000, 1001, 48000).string() + "\"}," +
                         "{\"url\":\"" + getSmpteDataPath(1920, 1080, 50, 1, 48000).string() + "\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},\"inUse\":false,"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}},"
                                              "{\"audio\":{\"sampleRate\":48000},\"inUse\":false,"
                                               "\"video\":{\"frameRate\":[30000,1001],\"height\":1080,\"width\":1920}},"
                                              "{\"audio\":{\"sampleRate\":48000},\"inUse\":false,"
                                               "\"video\":{\"frameRate\":[50,1],\"height\":1080,\"width\":1920}}]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, NonExistent, ioc)
{
    std::set<std::string> inUseUrls;
    Api::ProbeResource resource(ioc, inUseUrls);

    TestRequest request(Server::Request::Type::post, "[{\"url\":\"squiggle\"}]");
    co_await testResource(resource, request, "[null]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, ExistentAndNonExistent, ioc)
{
    std::string path = getSmpteDataPath(1920, 1080, 25, 1, 48000).string();
    Ffmpeg::ProbeResult probeResult = co_await Ffmpeg::ffprobe(ioc, path);

    std::set<std::string> inUseUrls;
    Api::ProbeResource resource(ioc, inUseUrls);

    TestRequest request(Server::Request::Type::post,
                        "[{\"url\":\"" + path + "\"}," +
                         "{\"url\":\"squiggle\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},\"inUse\":false,"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}},"
                                              "null]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, InCache, ioc)
{
    std::string path = getSmpteDataPath(1920, 1080, 25, 1, 48000).string();
    std::set<std::string> inUseUrls = { path };
    Api::ProbeResource resource(ioc, inUseUrls);

    TestRequest request(Server::Request::Type::post, "[{\"url\":\"" + path + "\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},\"inUse\":true,"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}}]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, Conflict, ioc)
{
    std::string path = getSmpteDataPath(1920, 1080, 25, 1, 48000).string();
    Ffmpeg::ProbeResult probeResult = co_await Ffmpeg::ffprobe(ioc, path);

    std::set<std::string> inUseUrls = { path };
    Api::ProbeResource resource(ioc, inUseUrls);

    TestRequest request(Server::Request::Type::post, "[{\"url\":\"" + path + "\", \"arguments\": [\"Argument\"]}]");
    co_await testResourceError(resource, request, Server::ErrorKind::Conflict);
}

} // namespace
