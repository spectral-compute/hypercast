#pragma once

#include <memory>
#include "util/awaitable.hpp"

class IOContext;

/// @addtogroup asio
/// @{

/**
 * An event-like object for asynchronous IO.
 */
class Event final
{
public:
    ~Event();
    explicit Event(IOContext &ioc);

    Event(Event &&) = default;

    /**
     * Wait for the event to happen.
     *
     * Spurious wakeups are permitted.
     */
    Awaitable<void> wait() const;

    /**
     * Wake everything that's waiting on this event.
     */
    void notifyAll();

private:
    struct Timer;

    IOContext &ioc;
    mutable std::unique_ptr<Timer> timer;
};

/// @}
