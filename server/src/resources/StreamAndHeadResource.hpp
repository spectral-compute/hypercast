#pragma once

#include "server/Path.hpp"
#include "server/Resource.hpp"
#include "util/Event.hpp"
#include "util/File.hpp"

#include <deque>
#include <vector>

namespace Server
{

/**
 * A resource that streams from one connection (via PUT) to another (via GET), and optionally serves the beginning of
 * the stream.
 *
 * This is useful for things like separated ingest for sources that cannot be probed directly.
 *
 * Currently, this is always private and has no caching.
 */
class StreamAndHeadResource final : public Resource
{
public:
    ~StreamAndHeadResource() override;

    /**
     * Constructor :)
     *
     * @param streamName The name of the resource that can be PUT or GETted for the main stream.
     * @param headName The name of the resource that serves the first headSize bytes of the stream. Must not be the same
     *                 as the stream name unless empty and headSize is zero.
     * @param bufferSize The maximum amount of data to keep in the buffer.
     * @param headSize The amount of data to keep for the head data. If zero, then there is no head resource.
     * @param path The path of the file to write the received data to.
     */
    explicit StreamAndHeadResource(IOContext &ioc, Path streamPath, size_t bufferSize,
                                   Path headPath = {}, size_t headSize = 0, std::filesystem::path path = {});

    size_t getMaxPutRequestLength() const noexcept override;
    bool getAllowNonEmptyPath() const noexcept override;

private:
    /**
     * Handle GET requests.
     */
    Awaitable<void> getAsync(Response &response, Request &request) override;

    /**
     * Handle the PUT request.
     */
    Awaitable<void> putAsync(Response &response, Request &request) override;

    /**
     * Handle GET requests for the stream.
     */
    Awaitable<void> getStream(Response &response);

    /**
     * Handle GET requests for the head.
     */
    Awaitable<void> getHead(Response &response) const;

    /**
     * Validate a sub-resource path and tell if the path is for the stream or the head.
     *
     * @param path The path to inspect.
     */
    bool validatePathAndGetIsHead(const Path &path) const;

    /* The constructor arguments. */
    const Path streamPath;
    const size_t bufferSize;
    const Path headPath;
    const size_t headSize;

    /**
     * The event to notify when a new data part is available to any GET requests.
     */
    Event pushEvent;

    /**
     * The event to notify when a new data part has been read (and thus the buffer might have more space in it).
     */
    Event popEvent;

    /**
     * The data we've received for this segment.
     */
    std::deque<std::vector<std::byte>> buffer;

    /**
     * The amount of data in the buffer.
     */
    size_t bufferUsed = 0;

    /**
     * Whether the end of the stream has been reached.
     */
    bool ended = false;

    /**
     * Whether anything is currently putting the stream.
     */
    bool streamPutConnected = false;

    /**
     * Whether anything is currently getting the stream.
     */
    bool streamGetConnected = false;

    /**
     * The first N bytes of data received by the stream.
     */
    std::vector<std::byte> head;

    /**
     * The file to write the segment to as it's received.
     */
    Util::File file;
};

} // namespace Server
