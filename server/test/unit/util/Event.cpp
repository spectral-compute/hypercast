#include "util/Event.hpp"

#include "coro_test.hpp"

#include <gtest/gtest.h>

namespace
{

TEST(Event, WaitNotify)
{
    IOContext ioc;
    Event event(ioc);
    bool fired = false;

    // The wait happens first (this relies on boost::asio::co_spawn forming an in-order queue), so the notifyAll should
    // unblock the wait.
    testCoSpawn([&event, &fired]() -> Awaitable<void> {
        co_await event.wait();
        fired = true;
    }, ioc);
    testCoSpawn([&event]() -> Awaitable<void> {
        event.notifyAll();
        co_return;
    }, ioc);

    ioc.poll();
    EXPECT_TRUE(fired);
}

TEST(Event, WaitOnly)
{
    IOContext ioc;
    Event event(ioc);
    bool fired = false;

    testCoSpawn([&event, &fired]() -> Awaitable<void> {
        co_await event.wait();
        fired = true;
    }, ioc);

    ioc.poll();
    EXPECT_FALSE(fired);
}

TEST(Event, NotifyWait)
{
    IOContext ioc;
    Event event(ioc);
    bool fired = false;

    // The notifyAll happens first (this relies on boost::asio::co_spawn forming an in-order queue), so the wait should
    // happen after, and therefore be waiting for a new notifyAll.
    testCoSpawn([&event]() -> Awaitable<void> {
        event.notifyAll();
        co_return;
    }, ioc);
    testCoSpawn([&event, &fired]() -> Awaitable<void> {
        co_await event.wait();
        fired = true;
    }, ioc);

    ioc.poll();
    EXPECT_FALSE(fired);
}

} // namespace
