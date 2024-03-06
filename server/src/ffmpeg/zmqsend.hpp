#pragma once

#include <span>
#include <string_view>
#include "util/awaitable.hpp"

class IOContext;

namespace Ffmpeg
{

/**
 * A single command to send to an FFmpeg filter graph via ZMQ.
 */
struct ZmqCommand final
{
    /**
     * The filter graph target node.
     */
    std::string_view target;

    /**
     * The command to send to the target node.
     */
    std::string_view command;

    /**
     * The argument, if any, to give for the command.
     */
    std::string_view argument;
};

/**
 * Send commands to an FFmpeg filter graph via ZMQ.
 *
 * @param address The address of the filter node's ZMQ server.
 * @param commands The commands to send. They are sent atomically with respect to any other call to zmqsend with the
 *                 same address.
 * @param sequential Submit the commands sequentially. Otherwise, they're submitted in an unspecifed order.
 */
Awaitable<void> zmqsend(IOContext &ioc, std::string_view address, std::span<const ZmqCommand> commands,
                        bool sequential = false);

/**
 * @copydoc zmqsend
 */
Awaitable<void> zmqsend(IOContext &ioc, std::string_view address, std::initializer_list<ZmqCommand> commands,
                        bool sequential = false);

} // namespace Ffmpeg
