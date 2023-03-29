#include "SegmentIndexDescriptorResource.hpp"

#include "server/Response.hpp"
#include "util/json.hpp"

Dash::SegmentIndexResource::~SegmentIndexResource() = default;

Dash::SegmentIndexResource::SegmentIndexResource(unsigned int segmentIndex) :
    SynchronousNullaryResource(true), segmentIndex(segmentIndex), creationTime(std::chrono::steady_clock::now())
{
}

void Dash::SegmentIndexResource::getSync(Server::Response &response, const Server::Request &)
{
    auto now = std::chrono::steady_clock::now();
    response.setCacheKind(Server::CacheKind::ephemeral);
    response << Json::dump({
       { "age", std::chrono::duration_cast<std::chrono::milliseconds>(now - creationTime).count() },
       { "index", segmentIndex },
       { "indexWidth", 9 }
    });
}
