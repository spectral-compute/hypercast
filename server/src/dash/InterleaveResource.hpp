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
     * @param timestampIntervalMs The interval, in ms, between timestamps. The default is set to disable timestamps,
     *                            which is useful for testing.
     */
    explicit InterleaveResource(IOContext &ioc, Log::Log &log, unsigned int numStreams,
                                unsigned int timestampIntervalMs = ~0u);

    Awaitable<void> operator()(Server::Response &response, Server::Request &request) override;
    bool getAllowGet() const noexcept override;

    /**
     * Append data to a stream in the interleave.
     *
     * @param dataPart The data to append. The stream is ended if this is empty.
     * @param streamIndex The index of the stream within the interleave.
     */
    void addStreamData(std::span<const std::byte> dataPart, unsigned int streamIndex);

private:
    Log::Context log;

    /**
     * The number of streams in the interleave that haven't yet finished.
     */
    unsigned int numRemaimningStreams;

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
     */
    std::vector<std::vector<std::byte>> data;
};

} // namespace Dash
