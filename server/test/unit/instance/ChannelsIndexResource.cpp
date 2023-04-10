#include "instance/ChannelsIndexResource.hpp"

#include "configuration/configuration.hpp"

#include "coro_test.hpp"
#include "resources/TestResource.hpp"

namespace
{

CORO_TEST(ChannelsIndexResource, Simple, ioc)
{
    Config::Root config = {
        .channels = {
            { "live/NamedChannel", { .name = "Channel Name" } },
            { "live/UnnamedChannel", {} }
        }
    };

    Instance::ChannelsIndexResource resource(config.channels);
    TestRequest request(Server::Request::Type::get);
    co_await testResource(resource, request, "{\"/live/NamedChannel/info.json\":\"Channel Name\","
                                             "\"/live/UnnamedChannel/info.json\":null}",
                          "application/json", Server::CacheKind::ephemeral);
}

} // namespace
