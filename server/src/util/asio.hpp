#pragma once

#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

/* This lets us do things like co_await (a && b), where a and b are Awaitable<void>s. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wheader-hygiene"
using namespace boost::asio::experimental::awaitable_operators;
#pragma clang diagnostic pop

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
 * A wrapper around the boost::asio::awaitable coroutine return type.
 *
 * This is useful for the following reasons (not all of which are implemented yet):
 * 1. Its name is shorter than boost::asio::awaitable.
 * 2. It can more easily be prototyped (so we don't have to include the above headers which generate over 100k lines
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
class [[nodiscard]] Awaitable final : public boost::asio::awaitable<T>
{
public:
    Awaitable(boost::asio::awaitable<T> &&rhs) : boost::asio::awaitable<T>::awaitable(std::move(rhs)) {}
};

/// @}

namespace std
{

template <typename T, typename... Us>
struct coroutine_traits<::Awaitable<T>, Us...> : public coroutine_traits<boost::asio::awaitable<T>, Us...> {};

} // namespace std

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

/// @}
