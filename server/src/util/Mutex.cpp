#include "Mutex.hpp"

#include "util/asio.hpp"

Awaitable<Mutex::LockGuard> Mutex::lockGuard()
{
    co_await lock();
    co_return LockGuard(*this);
}

Awaitable<void> Mutex::lock()
{
    while (locked) {
        co_await event.wait();
    }
    locked = true;
}
