#include "StreamAndHeadResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"
#include "util/debug.hpp"

Server::StreamAndHeadResource::~StreamAndHeadResource() = default;

Server::StreamAndHeadResource::StreamAndHeadResource(IOContext &ioc, Path streamPath, size_t bufferSize,
                                                     Path headPath, size_t headSize, std::filesystem::path path) :
    Resource(false),
    streamPath(std::move(streamPath)), bufferSize(bufferSize), headPath(std::move(headPath)), headSize(headSize),
    pushEvent(ioc), popEvent(ioc), file(path.empty() ? Util::File() : Util::File(ioc, std::move(path), true, false))
{
    assert(this->headPath != this->streamPath || (this->headPath.empty() && headSize == 0));
    head.reserve(headSize);
}

size_t Server::StreamAndHeadResource::getMaxPutRequestLength() const noexcept
{
    return ~(size_t)0;
}

bool Server::StreamAndHeadResource::getAllowNonEmptyPath() const noexcept
{
    return true;
}

Awaitable<void> Server::StreamAndHeadResource::getAsync(Response &response, Request &request)
{
    assert(!request.getIsPublic());
    response.setCacheKind(CacheKind::none);
    return validatePathAndGetIsHead(request.getPath()) ? getHead(response) : getStream(response);
}

Awaitable<void> Server::StreamAndHeadResource::putAsync(Response &response, Request &request)
{
    assert(!request.getIsPublic());
    response.setCacheKind(CacheKind::none);
    if (validatePathAndGetIsHead(request.getPath())) {
        throw Error(ErrorKind::UnsupportedType, "Cannot put the stream head");
    }

    /* Only one client at a time. */
    if (streamPutConnected) {
        throw Error(ErrorKind::Conflict, "Client already connected");
    }
    streamPutConnected = true;

    /* Read the request's data. */
    while (true) {
        // Get the next piece of data for the segment.
        std::vector<std::byte> dataPart = co_await request.readSome();

        // End of stream.
        if (dataPart.empty()) {
            break;
        }

        // Append the data to the head if we don't have it all yet.
        if (head.size() < headSize) {
            size_t headChunkSize = std::min(headSize - head.size(), dataPart.size());
            head.insert(head.end(), dataPart.data(), dataPart.data() + headChunkSize);
        }

        // Wait for space in the buffer.
        while (bufferUsed > 0 && bufferUsed + dataPart.size() > bufferSize) {
            co_await popEvent.wait();
        }

        // Add the data to the buffer.
        bufferUsed += dataPart.size();
        if (file) {
            buffer.emplace_back(dataPart);
        }
        else {
            buffer.emplace_back(std::move(dataPart));
        }

        // Notify anything that's waiting for more data that it's now available.
        pushEvent.notifyAll();

        // Write to the file if we're given one.
        if (file) {
            co_await file.write(dataPart);
        }
    }

    /* Notify anything that's waiting that we're at the end of the stream. */
    ended = true;
    pushEvent.notifyAll();
}

Awaitable<void> Server::StreamAndHeadResource::getStream(Response &response)
{
    /* Only one client at a time. */
    if (streamGetConnected) {
        throw Error(ErrorKind::Conflict, "Client already connected");
    }
    streamGetConnected = true;

    /* Keep serving for as long as we can. */
    while (!ended || !buffer.empty()) {
        // Wait until we have some data.
        if (buffer.empty()) {
            assert(!ended);
            co_await pushEvent.wait();
            continue;
        }

        // Write the next available data.
        response << buffer.front();

        // Notify that there's now more room in the buffer.
        bufferUsed -= buffer.front().size();
        buffer.pop_front();
        popEvent.notifyAll();

        // Flush the data so that something else has a chance to run and so that the client receives it in a timely
        // manner.
        co_await response.flush();
    }
}

Awaitable<void> Server::StreamAndHeadResource::getHead(Response &response) const
{
    /* Keep returning more data from the head until either the entire expected head has been emitted or the entire
       received head has been emitted and no more data is expected. */
    for (size_t i = 0; i < head.size() || (!ended && i < headSize);) {
        // Wait for more data if we need it.
        if (i == head.size()) {
            co_await pushEvent.wait();
            continue;
        }

        // Write the newly available data.
        response << std::span(head).subspan(i);
        i = head.size(); // We've now written this far.

        // Flush what we have so far.
        co_await response.flush();
    }
}

bool Server::StreamAndHeadResource::validatePathAndGetIsHead(const Path &path) const
{
    /* Route to the correct sub-resource based on the path. */
    if (path == streamPath) {
        return false;
    }
    if (headSize > 0 && path == headPath) {
        return true;
    }

    /* Does not exist. */
    throw Error(ErrorKind::NotFound, headSize ? "Neither stream nor head requested" : "Stream not requested");
}
