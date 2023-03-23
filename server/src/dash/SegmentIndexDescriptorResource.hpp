#pragma once

#include "server/SynchronousResource.hpp"

#include <chrono>

namespace Dash
{

class DashResources;
class InterleaveResource;

/**
 * A resource that help to keep the client synchronized with the available segments, even if they drift slightly over
 * time.
 */
class SegmentIndexResource final : public Server::SynchronousNullaryResource
{
public:
    ~SegmentIndexResource() override;

    /**
     * Create a resource that tells clients that request it that the given segment became available now.
     *
     */
    explicit SegmentIndexResource(unsigned int segmentIndex);

    void getSync(Server::Response &response, const Server::Request &request) override;

private:
    const unsigned int segmentIndex;
    const std::chrono::steady_clock::time_point creationTime; ///< When the resource (and thus segment) was created.
};

} // namespace Dash
