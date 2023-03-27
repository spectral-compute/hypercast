#include "SegmentResource.hpp"

#include "DashResources.hpp"
#include "InterleaveResource.hpp"

#include "configuration/configuration.hpp"
#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/debug.hpp"
#include "util/json.hpp"

Dash::SegmentResource::~SegmentResource() = default;

Dash::SegmentResource::SegmentResource(IOContext &ioc, Log::Log &log, const Config::Dash &config,
                                       Dash::DashResources &resources,
                                       unsigned int streamIndex, unsigned int segmentIndex,
                                       std::shared_ptr<InterleaveResource> interleave,
                                       unsigned int interleaveIndex, unsigned int indexInInterleave) :
    Resource(config.expose), log(log("segment")), event(ioc), resources(resources),
    streamIndex(streamIndex), segmentIndex(segmentIndex), interleave(std::move(interleave)),
    indexInInterleave(indexInInterleave)
{
    /* Log information about this segment. */
    this->log << "new" << Log::Level::info << Json::dump({
        { "streamIndex", streamIndex },
        { "segmentIndex", segmentIndex },
        { "interleaveIndex", interleaveIndex },
        { "indexInInterleave", indexInInterleave },
    });
}

Awaitable<void> Dash::SegmentResource::getAsync(Server::Response &response, Server::Request &request)
{
    if (!getIsPublic()) {
        throw Server::Error(Server::ErrorKind::Forbidden, "Not a public resource");
    }

    /* Keep waiting for more data until we've had it all. */
    for (size_t i = 0; ; i++) {
        // Wait for more data to become available if necessary.
        assert(i <= data.size());
        while (i == data.size()) {
            co_await event.wait();
        }
        assert(i < data.size());

        // Handle the end-of-file case.
        if (data[i].empty()) {
            break;
        }

        // Give the response the next piece of data.
        response << data[i];
    }
}

Awaitable<void> Dash::SegmentResource::putAsync(Server::Response &response, Server::Request &request)
{
    assert(!request.getIsPublic());
    response.setCacheKind(Server::CacheKind::none);

    /* Read the request's data. */
    for (bool first = true; ; first = false) {
        // Get the next piece of data for the segment.
        std::vector<std::byte> dataPart = co_await request.readSome();

        // Notify the resources (and log for ourselves) that we've started receiving.
        if (first) {
            resources.notifySegmentStart(streamIndex, segmentIndex);
            log << "start" << Log::Level::info;
        }

        // Hand the data over to the interleave.
        interleave->addStreamData(dataPart, indexInInterleave);

        // Record the data if it's useful, and notify anything waiting for it.
        bool isEmpty = dataPart.empty();
        if (getIsPublic()) {
            data.emplace_back(std::move(dataPart));
            event.notifyAll();
        }

        // Handle end of request body.
        if (isEmpty) {
            break;
        }
    }
}

size_t Dash::SegmentResource::getMaxRequestLength() const noexcept
{
    return 1zu << 32;
}
