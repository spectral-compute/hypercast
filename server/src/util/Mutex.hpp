#pragma once

#include "Event.hpp"

/// @addtogroup asio
/// @{

/**
 * A mutex-like object for asynchronous IO.
 */
class Mutex final
{
public:
    /**
     * Like std::lock_guard, but for this mutex class.
     *
     * This exists because std::lock_guard can't be co_awaited in its construction.
     */
    class [[nodiscard]] LockGuard final
    {
    public:
        ~LockGuard()
        {
            if (parent) {
                parent->unlock();
            }
        }

        LockGuard(LockGuard &&other) : parent(other.parent)
        {
            other.parent = nullptr;
        }

    private:
        friend class Mutex;

        explicit LockGuard(Mutex &parent) : parent(&parent) {}

        // No copying.
        LockGuard(const LockGuard &) = delete;
        LockGuard &operator=(const LockGuard &) = delete;

        // Don't allow move *assignment* (where we might have a mutex to unlock too) either. Only move construction.
        LockGuard &operator=(LockGuard &&other) = delete;

        /**
         * The mutex to unlock when this object goes out of scope.
         */
        Mutex *parent;
    };

    explicit Mutex(IOContext &ioc) : event(ioc) {}

    /**
     * Lock the mutex, and get a RAII object that unlocks it when it goes out of scope.
     */
    Awaitable<LockGuard> lockGuard();

    /**
     * Lock the mutex.
     */
    Awaitable<void> lock();

    /**
     * Unlock the mutex.
     */
    void unlock()
    {
        locked = false;
        event.notifyAll();
    }

private:
    Event event;
    bool locked = false;
};

/// @}
