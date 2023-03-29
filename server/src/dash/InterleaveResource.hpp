#pragma once

#include "log/Log.hpp"
#include "server/Resource.hpp"
#include "util/Event.hpp"

#include <chrono>
#include <span>
#include <vector>

namespace Dash
{

/**
 * A RISE interleave segment.
 */
class InterleaveResource final : public Server::Resource
{
public:
    ~InterleaveResource() override;

    /**
     * Constructor :)
     *
     * @param numStreams The number of streams to include in the interleave.
     * @param minInterleaveBytesPerWindow The minimum amount of data, in bytes, to emit per interleave window. The
     *                                    default value is set to disable the minimum rate, which is useful for testing.
     * @param minInterleaveWindowMs The window over which to calculate the actual interleave rate in ms. The default
     *                              value is set to disable the minimum rate, which is useful for testing.
     * @param timestampIntervalMs The interval, in ms, between timestamps. The default is set to disable timestamps,
     *                            which is useful for testing.
     */
    explicit InterleaveResource(IOContext &ioc, Log::Log &log, unsigned int numStreams,
                                unsigned int minInterleaveBytesPerWindow = 0, unsigned int minInterleaveWindowMs = ~0u,
                                unsigned int timestampIntervalMs = ~0u);

    Awaitable<void> getAsync(Server::Response &response, Server::Request &request) override;

    /**
     * Append data to a stream in the interleave.
     *
     * @param dataPart The data to append. The stream is ended if this is empty.
     * @param streamIndex The index of the stream within the interleave.
     */
    void addStreamData(std::span<const std::byte> dataPart, unsigned int streamIndex);

private:
    /**
     * Figure out how much extra data, in bytes, is needed to meet the minimum interleave rate.
     *
     * This method must not be called until at least one data chunk has been added to the stream.
     */
    unsigned int getPaddingDataLengthForWindow(std::chrono::steady_clock::time_point now) const;

    Log::Context log;

    /**
     * The number of streams in the interleave that haven't yet finished.
     */
    unsigned int numRemainingStreams;

    /**
     * The minimum amount of data, in bytes, to emit per interleave window.
     */
    const unsigned int minInterleaveBytesPerWindow;

    /**
     * The window over which to calculate the actual interleave rate in ms.
     */
    const unsigned int minInterleaveWindowMs;

    /**
     * The interval, in ms, between timestamps.
     */
    const unsigned int timestampIntervalMs;

    /**
     * When the last timestamp was injected into the interleave.
     */
    std::chrono::steady_clock::time_point lastTimestamp;

    /**
     * The event to notify when a new data part is available to any GET requests.
     */
    Event event;

    /**
     * The data we've received for this interleave.
     *
     * Each element is a pair: { received data, time the data is received }.
     */
    std::vector<std::pair<std::vector<std::byte>, std::chrono::steady_clock::time_point>> data;
};

} // namespace Dash
