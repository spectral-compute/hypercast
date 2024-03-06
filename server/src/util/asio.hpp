#pragma once

#include "log/Level.hpp"

#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>

#include <functional>
#include <span>
#include "awaitable.hpp"

/* This lets us do things like co_await (a && b), where a and b are Awaitable<void>s. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wheader-hygiene"
using namespace boost::asio::experimental::awaitable_operators;
#pragma clang diagnostic pop

namespace Log
{

class Context;

} // namespace Log

/**
 * @defgroup asio Asynchronous IO
 *
 * Stuff that wraps boost::asio.
 *
 * This wrapper is useful because it lets us omit the Boost headers in most places and give us more control.
 * @see Awaitable.
 */

/// @addtogroup asio
/// @{

/**
 * A wrapper around boost::asio::io_context.
 *
 * This is useful for the following reasons:
 * 1. Its name is shorter than boost::asio::io_context.
 * 2. It can more easily be prototyped (so we don't have to include the above headers with generate over 100k lines
 *    after preprocessing).
 * 3. Having an implementation like this means we can change the details or override the implementation.
 */
class IOContext final : public boost::asio::io_context
{
public:
    using boost::asio::io_context::io_context;
};

/**
 * Spawn a detached coroutine.
 *
 * @param fn The function to run in the coroutine.
 */
template<typename F>
void spawnDetached(IOContext &ioc, F&& fn)
{
    boost::asio::co_spawn((boost::asio::io_context &)ioc, std::forward<F>(fn), boost::asio::detached);
}

/**
 * Spawn a detached coroutine.
 *
 * @param fn The function to run in the coroutine.
 */
void spawnDetached(IOContext &ioc, Log::Context &log, std::function<Awaitable<void>()> fn,
                   Log::Level level = Log::Level::error);

/**
 * Wait for all of a set of void awaitables of unknown length in parallel.
 *
 * Use && to wait for a fixed number of awaitables.
 */
Awaitable<void> awaitTree(std::span<Awaitable<void>> awaitables);

/// @}
