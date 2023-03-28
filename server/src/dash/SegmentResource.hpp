#pragma once

#include "log/Log.hpp"
#include "server/Resource.hpp"
#include "util/Event.hpp"
#include "util/File.hpp"

#include <vector>

namespace Config
{

struct Dash;

} // namespace Config

namespace Dash
{

class DashResources;
class InterleaveResource;

/**
 * A DASH segment.
 */
class SegmentResource final : public Server::Resource
{
public:
    ~SegmentResource() override;

    /**
     * Constructor :)
     *
     * @param resources The coordination object.
     * @param streamIndex The index of the stream to which this segment belongs.
     * @param segmentIndex The index of this segment.
     * @param interleave The interleave that this segment is to be interleaved into.
     * @param interleaveIndex The index of the interleave (not the interleave segment, which is the same as the segment
     *                        index).
     * @param indexInInterleave The index of this stream in the interleave.
     * @param path The path of the file to write the received data to.
     */
    explicit SegmentResource(IOContext &ioc, Log::Log &log, const Config::Dash &config, DashResources &resources,
                             unsigned int streamIndex, unsigned int segmentIndex,
                             std::shared_ptr<InterleaveResource> interleave,
                             unsigned int interleaveIndex, unsigned int indexInInterleave, std::filesystem::path path);

    size_t getMaxRequestLength() const noexcept override;

private:
    /**
     * Handle GET requests for this segment.
     */
    Awaitable<void> getAsync(Server::Response &response, Server::Request &request) override;

    /**
     * Handle the PUT request for this segment.
     */
    Awaitable<void> putAsync(Server::Response &response, Server::Request &request) override;

    Log::Context log;

    /**
     * The event to notify when a new data part is available to any GET requests.
     */
    Event event;

    /**
     * The parent resources object we should notify when we start receiving data.
     */
    DashResources &resources;

    /**
     * The index of the stream this segment belongs go.
     */
    const unsigned int streamIndex;

    /**
     * The index of this segment within its stream.
     */
    const unsigned int segmentIndex;

    /**
     * The interleave this segment gets interleaved into.
     */
    std::shared_ptr<InterleaveResource> interleave;

    /**
     * The index of this segment's stream in the interleave.
     */
    const unsigned int indexInInterleave;

    /**
     * The data we've received for this segment.
     */
    std::vector<std::vector<std::byte>> data;

    /**
     * The file to write the segment to as it's received.
     */
    Util::File file;
};

} // namespace Dash
