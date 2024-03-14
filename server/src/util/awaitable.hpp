#pragma once


/// @addtogroup asio
/// @{

namespace boost::asio {
    class any_io_executor;
    template <typename T, typename Executor>
    class awaitable;
} // namespace boost::asio

/**
 * A wrapper around the boost::asio::awaitable coroutine return type.
 *
 * This is useful for the following reasons (not all of which are implemented yet):
 * 1. Its name is shorter than boost::asio::awaitable.
 * 2. It can more easily be prototyped (so we don't have to include the asio headers which generate over 100k lines
 *    after preprocessing).
 * 3. By having to include this header to do more than declare a function taking one of these classes, we avoid having a
 *    lot of code emitted into translation units accidentally by inline methods.
 * 4. Having an implementation like this means we can change the details or override the implementation.
 * 5. As a special case of (4), we can probably make this so that it code-generates the coroutine loop in one place
 *    only, rather than everywhere a co_await appears.
 *
 * @tparam T The return type for the coroutine.
 */
template <typename T>
using Awaitable = boost::asio::awaitable<T, boost::asio::any_io_executor>;

/// @}
