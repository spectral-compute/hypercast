#include "api/ProbeResource.hpp"

#include "ffmpeg/ProbeCache.hpp"

#include "coro_test.hpp"
#include "data.hpp"
#include "resources/TestResource.hpp"

namespace
{

CORO_TEST(ApiProbeResource, Simple, ioc)
{
    Ffmpeg::ProbeCache probeCache;
    Api::ProbeResource resource(ioc, probeCache);
    TestRequest request(Server::Request::Type::post,
                        "[{\"url\":\"" + getSmpteDataPath(1920, 1080, 25, 1, 48000).string() + "\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}}]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, Multiple, ioc)
{
    Ffmpeg::ProbeCache probeCache;
    Api::ProbeResource resource(ioc, probeCache);
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

CORO_TEST(ApiProbeResource, NonExistent, ioc)
{
    Ffmpeg::ProbeCache probeCache;
    Api::ProbeResource resource(ioc, probeCache);
    TestRequest request(Server::Request::Type::post, "[{\"url\":\"squiggle\"}]");
    co_await testResource(resource, request, "[null]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, ExistentAndNonExistent, ioc)
{
    Ffmpeg::ProbeCache probeCache;
    Api::ProbeResource resource(ioc, probeCache);
    TestRequest request(Server::Request::Type::post,
                        "[{\"url\":\"" + getSmpteDataPath(1920, 1080, 25, 1, 48000).string() + "\"}," +
                         "{\"url\":\"squiggle\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":48000},"
                                               "\"video\":{\"frameRate\":[25,1],\"height\":1080,\"width\":1920}},"
                                              "null]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, InCache, ioc)
{
    std::string path = getSmpteDataPath(1920, 1080, 25, 1, 48000).string();

    Ffmpeg::ProbeCache probeCache;
    probeCache.insert({ .video = MediaInfo::VideoStreamInfo{}, .audio = MediaInfo::AudioStreamInfo{} }, path, {});

    Api::ProbeResource resource(ioc, probeCache);
    TestRequest request(Server::Request::Type::post, "[{\"url\":\"" + path + "\"}]");
    co_await testResource(resource, request, "[{\"audio\":{\"sampleRate\":0},"
                                               "\"video\":{\"frameRate\":[0,0],\"height\":0,\"width\":0}}]",
                          "application/json");
}

CORO_TEST(ApiProbeResource, Conflict, ioc)
{
    std::string path = getSmpteDataPath(1920, 1080, 25, 1, 48000).string();

    Ffmpeg::ProbeCache probeCache;
    probeCache.insert({ .video = MediaInfo::VideoStreamInfo{}, .audio = MediaInfo::AudioStreamInfo{} }, path,
                      { "Argument" });

    Api::ProbeResource resource(ioc, probeCache);
    TestRequest request(Server::Request::Type::post, "[{\"url\":\"" + path + "\"}]");
    co_await testResourceError(resource, request, Server::ErrorKind::Conflict);
}

} // namespace
