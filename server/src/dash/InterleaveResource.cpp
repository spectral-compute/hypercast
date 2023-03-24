#include "InterleaveResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"

namespace
{

/**
 * Convert from an integer to a little-endian sequence of bytes.
 */
void writeLittleEndianInteger(std::byte (&dst)[8], uint64_t src)
{
    for (size_t i = 0; i < sizeof(dst); i++) {
        dst[i] = (std::byte)((src >> (i * 8)) & 0xFF);
    }
}

} // namespace

static_assert(std::ratio_less_equal<std::chrono::system_clock::period, std::micro>::value,
              "System clock resolution is insufficient.");

Dash::InterleaveResource::~InterleaveResource() = default;

Dash::InterleaveResource::InterleaveResource(IOContext &ioc, Log::Log &log, unsigned int numStreams,
                                             unsigned int timestampIntervalMs) :
    log(log("interleave")), numRemainingStreams(numStreams), timestampIntervalMs(timestampIntervalMs), event(ioc)
{
}

Awaitable<void> Dash::InterleaveResource::getAsync(Server::Response &response, Server::Request &request)
{
    /* Give the response all the data we've got for the interleave so far. */
    for (const auto &chunk: data) {
        response << chunk;
        co_await response.flush();
    }

    /* Keep waiting for more data until we've had it all. */
    for (size_t i = 0; numRemainingStreams > 0; i++) {
        // Wait for more data to become available if necessary.
        assert(i <= data.size());
        while (i == data.size()) {
            co_await event.wait();
        }
        assert(i < data.size());

        // Give the response the next piece of data.
        response << data[i];
        co_await response.flush();
    }
}

void Dash::InterleaveResource::addStreamData(std::span<const std::byte> dataPart, unsigned int streamIndex)
{
    assert(streamIndex < 31);
    assert(numRemainingStreams > 0);

    /* Record if this stream is ending. */
    if (dataPart.empty()) {
        numRemainingStreams--;
    }
    // Even if the stream is ending, we need to put an empty chunk (with its header) into the interleave so that the
    // client knows it's ended.

    /* Calculate the length. */
    unsigned int lengthId = 0;
    unsigned int lengthByteCount = 0;
    std::byte lengthBytes[8];

    // Calculate the length as a little-endian value.
    writeLittleEndianInteger(lengthBytes, dataPart.size());

    // Figure out the length ID.
    if (dataPart.size() < 1 << 8) {
        lengthId = 0;
    }
    else if (dataPart.size() < 1 << 16) {
        lengthId = 1;
    }
    else if (dataPart.size() < (size_t)1 << 32) {
        lengthId = 2;
    }
    else {
        lengthId = 3;
    }

    // Compute the number of bytes for the length for the given length ID.
    lengthByteCount = 1 << lengthId;

    /* Figure out whether there should be a timestamp. */
    bool addTimestamp = false;
    std::byte timestampBytes[8];
    if (timestampIntervalMs < ~0u) {
        auto now = std::chrono::steady_clock::now();
        addTimestamp = now - lastTimestamp >= std::chrono::milliseconds(timestampIntervalMs);
        if (addTimestamp) {
            lastTimestamp = now;
            auto sysNow = std::chrono::system_clock::now();
            uint64_t utcµs = std::chrono::round<std::chrono::microseconds>(sysNow.time_since_epoch()).count();
            writeLittleEndianInteger(timestampBytes, utcµs);
        }
    }

    /* Calculate the content ID. */
    std::byte contentId = (std::byte)(streamIndex | (addTimestamp ? 1 << 5 : 0) | (lengthId << 6));

    /* Build the complete chunk. */
    // Create a block of memory of the correct size.
    unsigned int dataPartOffset = 1 + lengthByteCount + (addTimestamp ? sizeof(timestampBytes) : 0);
    std::vector<std::byte> chunk(dataPartOffset + dataPart.size());

    // Copy the content ID into the chunk.
    chunk[0] = contentId;

    // Copy the length into the chunk.
    memcpy(chunk.data() + 1, lengthBytes, lengthByteCount);

    // Copy the timestamp into the chunk.
    if (addTimestamp) {
        memcpy(chunk.data() + 1 + lengthByteCount, timestampBytes, sizeof(timestampBytes));
    }

    // Copy the data itself.
    memcpy(chunk.data() + dataPartOffset, dataPart.data(), dataPart.size());

    /* Append the chunk and notify anything that's waiting for it. */
    data.emplace_back(std::move(chunk));
    event.notifyAll();
}
