#include "Event.hpp"

#include "asio.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/steady_timer.hpp>

/* Unfortunately, boost::asio doesn't seem to provide any mechanism to await non-exceptionally on something that's
   notified from another coroutine. This Event class is based on the idea of using a Boost timer from this post:
   https://stackoverflow.com/a/17029022. */

/**
 * A timer object that waits (almost) indefinitely.
 */
struct Event::Timer final
{
    explicit Timer(IOContext &ioc) : timer(ioc)
    {
        /* The std::chrono::years value is guaranteed to allow at least this. */
        timer.expires_from_now(std::chrono::years(40000));
    }

    boost::asio::steady_timer timer;
};

Event::~Event() = default;
Event::Event(IOContext &ioc) : ioc(ioc) {}

Awaitable<void> Event::wait() const
{
    if (!timer) {
        timer = std::make_unique<Timer>(ioc);
    }

    // The use of boost::asio::as_tuple makes this return normally even if an exception (due to timer cancellation)
    // happens.
    co_await timer->timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
}

void Event::notifyAll()
{
    if (!timer) {
        return;
    }
    timer->timer.cancel();
    timer.reset();
}
