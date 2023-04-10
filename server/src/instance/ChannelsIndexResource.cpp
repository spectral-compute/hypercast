#include "ChannelsIndexResource.hpp"

#include "configuration/configuration.hpp"
#include "server/Path.hpp"
#include "server/Response.hpp"
#include "util/json.hpp"

Instance::ChannelsIndexResource::~ChannelsIndexResource() = default;

void Instance::ChannelsIndexResource::getSync(Server::Response &response, const Server::Request &)
{
    response.setCacheKind(Server::CacheKind::ephemeral);
    nlohmann::json j;
    for (const auto &[path, channel]: channels) {
        j["/" + (std::string)(Server::Path(path) / "info.json")] = channel.name.empty() ?
                                                                   nlohmann::json(nullptr) : channel.name;
    }
    response.setMimeType("application/json");
    response << Json::dump(j);
}
