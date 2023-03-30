#include "InterleaveResource.hpp"

#include "server/Request.hpp"
#include "server/Response.hpp"
#include "util/asio.hpp"

#include <random>

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

/**
 * Generate some random data.
 *
 * @param length The amount of random data to generate, in bytes.
 * @return A least length bytes of data. This may be rounded up by a small amount for efficiency.
 */
std::vector<std::byte> getRandomData(unsigned int length)
{
    /* Persistent PRNG state. */
    // This uses 32-bit arithmetic because std::minstd_rand has values that are chosen for 32-bit arithmetic.
    static std::minstd_rand engine;
    static std::uniform_int_distribution<uint32_t> distribution(0, ~(uint32_t)0);

    /* Figure out how many 32-bit values are needed to achieve a length of at least that given. */
    unsigned int n = (length + 3) / 4;

    /* Return the random data. */
    std::vector<std::byte> result(n * 4);
    for (unsigned int i = 0; i < n; i++) {
        uint32_t value = distribution(engine);
        memcpy(result.data() + i * 4, &value, 4);
    }
    return result;
}

} // namespace

static_assert(std::ratio_less_equal<std::chrono::system_clock::period, std::micro>::value,
              "System clock resolution is insufficient.");

Dash::InterleaveResource::~InterleaveResource() = default;

Dash::InterleaveResource::InterleaveResource(IOContext &ioc, Log::Log &log, unsigned int numStreams,
                                             unsigned int minInterleaveBytesPerWindow,
                                             unsigned int minInterleaveWindowMs, unsigned int timestampIntervalMs) :
    Resource(true), log(log("interleave")), numRemainingStreams(numStreams),
    minInterleaveBytesPerWindow(minInterleaveBytesPerWindow), minInterleaveWindowMs(minInterleaveWindowMs),
    timestampIntervalMs(timestampIntervalMs), event(ioc)
{
}

Awaitable<void> Dash::InterleaveResource::getAsync(Server::Response &response, Server::Request &request)
{
    /* Give the response all the data we've got for the interleave so far. */
    for (const auto &[chunk, timeReceived]: data) {
        response << chunk;
        co_await response.flush();
    }

    /* Keep waiting for more data until we've had it all. */
    for (size_t i = 0; !hasEnded(); i++) {
        // Wait for more data to become available if necessary.
        assert(i <= data.size());
        while (i == data.size()) {
            co_await event.wait();
        }
        assert(i < data.size());

        // Give the response the next piece of data.
        response << data[i].first;
        co_await response.flush();
    }
}

void Dash::InterleaveResource::addStreamData(std::span<const std::byte> dataPart, unsigned int streamIndex)
{
    assert(streamIndex < maxStreams);
    assert(numRemainingStreams > 0);

    /* We need to know when the chunk was received for some of the realtime stuff below. */
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    /* Record if this stream is ending. */
    if (dataPart.empty()) {
        numRemainingStreams--;
    }
    // Even if the stream is ending, we need to put an empty chunk (with its header) into the interleave so that the
    // client knows it's ended.

    /* Figure out whether there should be a timestamp. */
    bool addTimestamp = false;
    if (timestampIntervalMs < ~0u) {
        if (now - lastTimestamp >= std::chrono::milliseconds(timestampIntervalMs)) {
            lastTimestamp = now;
            addTimestamp = true;
        }
    }

    /* Append the chunk and notify anything that's waiting for it. */
    addChunk(dataPart, streamIndex, now, addTimestamp);

    /* Pad the interleave with extra data if needed to maintain the minimum rate. */
    // We can't (and shouldn't) append extra data if the stream is ending anyway. The CDN should flush its buffers in
    // that case.
    if (hasEnded()) {
        return;
    }

    // Figure out how much data we need to pad.
    unsigned int extraData = getPaddingDataLengthForWindow(now);

    // Don't generate a padding chunk if we don't need any data.
    if (extraData == 0) {
        return;
    }

    // We use random data to make sure there's no compression anywhere that reduces the effective rate. The length
    // doesn't account for the size of the chunk header for the random data, so this can be a few bytes over, but that's
    // OK.
    addControlChunk(ControlChunkType::discard, getRandomData(extraData), now);
}

void Dash::InterleaveResource::addControlChunk(ControlChunkType type, std::span<const std::byte> chunkData)
{
    addControlChunk(type, chunkData, std::chrono::steady_clock::now());
}

void Dash::InterleaveResource::addChunk(std::span<const std::byte> dataPart, unsigned int streamIndex,
                                        std::chrono::steady_clock::time_point now, bool addTimestamp,
                                        std::span<const std::byte> prefixData)
{
    assert(streamIndex < maxStreams + 1);

    /* Calculate the length. */
    unsigned int lengthId = 0;
    unsigned int lengthByteCount = 0;
    std::byte lengthBytes[8];

    // Calculate the length as a little-endian value.
    uint64_t chunkDataSize = prefixData.size() + dataPart.size();
    writeLittleEndianInteger(lengthBytes, chunkDataSize);

    // Figure out the length ID.
    if (chunkDataSize < 1 << 8) {
        lengthId = 0;
    }
    else if (chunkDataSize < 1 << 16) {
        lengthId = 1;
    }
    else if (chunkDataSize < (size_t)1 << 32) {
        lengthId = 2;
    }
    else {
        lengthId = 3;
    }

    // Compute the number of bytes for the length for the given length ID.
    lengthByteCount = 1 << lengthId;

    /* Compute the timestamp. */
    std::byte timestampBytes[8];
    if (addTimestamp) {
        std::chrono::system_clock::time_point sysNow = std::chrono::system_clock::now();
        uint64_t utcµs = std::chrono::round<std::chrono::microseconds>(sysNow.time_since_epoch()).count();
        writeLittleEndianInteger(timestampBytes, utcµs);
    }

    /* Calculate the content ID. */
    std::byte contentId = (std::byte)(streamIndex | (addTimestamp ? 1 << 5 : 0) | (lengthId << 6));

    /* Build the complete chunk. */
    // Create a block of memory of the correct size.
    unsigned int chunkDataOffset = 1 + lengthByteCount + (addTimestamp ? sizeof(timestampBytes) : 0);
    size_t dataPartOffset = chunkDataOffset + prefixData.size();
    std::vector<std::byte> chunk(dataPartOffset + dataPart.size());

    // Copy the content ID into the chunk.
    chunk[0] = contentId;

    // Copy the length into the chunk.
    memcpy(chunk.data() + 1, lengthBytes, lengthByteCount);

    // Copy the timestamp into the chunk.
    if (addTimestamp) {
        memcpy(chunk.data() + 1 + lengthByteCount, timestampBytes, sizeof(timestampBytes));
    }

    // Copy the prefix data;
    memcpy(chunk.data() + chunkDataOffset, prefixData.data(), prefixData.size());

    // Copy the data itself.
    memcpy(chunk.data() + dataPartOffset, dataPart.data(), dataPart.size());

    /* Append the chunk to the list of chunks and notify anything that's waiting that we have a new chunk. */
    data.emplace_back(chunk, now);
    event.notifyAll();
}

void Dash::InterleaveResource::addControlChunk(ControlChunkType type, std::span<const std::byte> chunkData,
                                               std::chrono::steady_clock::time_point now)
{
    assert(!hasEnded());
    std::byte controlChunkHeader = (std::byte)type;
    addChunk(chunkData, maxStreams, now, false, std::span(&controlChunkHeader, 1));
}

unsigned int Dash::InterleaveResource::getPaddingDataLengthForWindow(std::chrono::steady_clock::time_point now) const
{
    assert(!data.empty());

    /* No data is needed if the minimum interleave rate is zero. */
    if (minInterleaveBytesPerWindow == 0) {
        return 0;
    }

    /* Figure out when the start of the window to consider is. */
    std::chrono::steady_clock::time_point windowStart =
        now - std::chrono::duration_cast<std::chrono::steady_clock::duration>
              (std::chrono::milliseconds(minInterleaveWindowMs));

    /* If the earliest chunk is still in the window (but not at its earliest edge), then we might receive more real
       data. */
    if (data.front().second > windowStart) {
        return 0;
    }

    /* Figure out how much data is in that window by iterating the received data, starting from the end. */
    size_t dataInWindow = 0;
    for (auto it = data.rbegin(); it != data.rend(); ++it) {
        const auto &[pastChunk, timeReceived] = *it;

        // If we've gone past the start of the window, the summation is complete.
        if (timeReceived < windowStart) {
            break;
        }

        // Add the amount of data received in this chunk.
        dataInWindow += pastChunk.size();

        // If we've exceeded the amount of data needed, we don't need to add any extra.
        if (dataInWindow >= minInterleaveBytesPerWindow) {
            return 0;
        }
    }

    /* Return the amount of extra data we need. */
    assert(dataInWindow < minInterleaveBytesPerWindow);
    return (unsigned int)(minInterleaveBytesPerWindow - dataInWindow);
}
