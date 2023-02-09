#include "Log.hpp"

#include "util/asio.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <cassert>

Log::Context::NewItem::~NewItem()
{
    parent.append(*this);
}

Log::Context::NewItem::NewItem(Context &parent, Level level, std::string_view kind) :
    steadyCreationTime(std::chrono::steady_clock::now()), systemCreationTime(std::chrono::system_clock::now()),
    parent(parent), level(level), kind(kind)
{
}

Log::Context::~Context()
{
    auto now = std::chrono::steady_clock::now();
    parent.append({
        .logTime = now - parent.steadyCreationTime,
        .contextTime = now - steadyCreationTime,
        .systemTime = std::chrono::system_clock::now(),
        .level = Level::info,
        .kind = "log context",
        .message = "destroyed",
        .contextName = std::move(name),
        .contextIndex = index
    });
}

Log::Context::Context(Log &parent, std::string name, size_t index) :
    steadyCreationTime(std::chrono::steady_clock::now()), parent(parent), name(std::move(name)), index(index)
{
    parent.append({
        .logTime = steadyCreationTime - parent.steadyCreationTime,
        .contextTime = std::chrono::steady_clock::duration(0),
        .systemTime = std::chrono::system_clock::now(),
        .level = Level::info,
        .kind = "log context",
        .message = "created",
        .contextName = this->name,
        .contextIndex = index
    });
}

void Log::Context::append(NewItem &newItem)
{
    parent.append({
        .logTime = newItem.steadyCreationTime - parent.steadyCreationTime,
        .contextTime = newItem.steadyCreationTime - steadyCreationTime,
        .systemTime = newItem.systemCreationTime,
        .level = newItem.level,
        .kind = std::move(newItem.kind),
        .message = newItem.stream.str(),
        .contextName = name,
        .contextIndex = index
    });
}

Log::Log::~Log() = default;

Log::Log::Log(Level minLevel, bool print, IOContext &ioc) :
    ioc(ioc),
    steadyCreationTime(std::chrono::steady_clock::now()), systemCreationTime(std::chrono::system_clock::now()),
    minLevel(minLevel), print(print), event(ioc)
{
}

Log::Context Log::Log::operator()(std::string_view name)
{
    assert(!name.empty()); // Require a name.

    /* Find the context index counter if it exists. */
    std::string nameStr(name.data(), name.size());
    auto it = contextNextIndices.find(nameStr);

    /* If it doesn't exist, create it. */
    if (it == contextNextIndices.end()) {
        it = contextNextIndices.insert({nameStr, 0}).first;
    }

    /* Create a new context with the right index. */
    return Context(*this, std::move(nameStr), (*it).second++);
}

Awaitable<Log::Item> Log::Log::operator[](size_t index) const
{
    assert(index < writtenItems + queue.size());

    /* If the item is still in the queue, just return a copy of it. */
    if (index >= writtenItems) {
        co_return queue[index - writtenItems];
    }

    /* The item has already left the queue, so get it from where we sent it. */
    co_return co_await load(index);
}

Awaitable<void> Log::Log::wait() const
{
    return event.wait();
}

void Log::Log::append(Item item)
{
    /* Since we couldn't write the creation log entry in the constructor, write it now. */
    if (writtenItems == 0 && queue.empty() && minLevel <= Level::info) {
        scheduleAppend({
            .logTime = std::chrono::steady_clock::duration(0),
            .contextTime = std::chrono::steady_clock::duration(0),
            .systemTime = std::chrono::system_clock::now(),
            .level = Level::info,
            .kind = "log",
            .message = "created",
            .contextName = "",
            .contextIndex = 0
        });
    }

    /* Ignore items whose level is lower than the minimum level. */
    if (item.level < minLevel) {
        return;
    }

    /* Write the actual log entry. */
    scheduleAppend(std::move(item));
}

void Log::Log::scheduleAppend(Item item)
{
    /* Format the message to stderr if we're printing. */
    if (print) {
        fprintf(stderr, "%s\n", item.format(true).c_str());
    }

    /* Add the item to the queue. */
    queue.emplace_back(std::move(item));

    /* Notify the event now. */
    // Doing this before scheduling the storage of the queue means it's likely the fast path of operator[] returning
    // from the queue will be hit. Also, we should do this evne if the scheduler coroutine has already been spawned.
    event.notifyAll();

    /* If there's an outstanding writer coroutine, leave it to write what we just added. Otherwise, create one. */
    // If our item is the only one, then there won't be a coroutine in flight to do the writing. Otherwise, there will
    // be from whatever created the already existing items.
    if (queue.size() > 1) {
        return;
    }

    // New coroutine to actually call store, even though scheduleAppend isn't a coroutine.
    boost::asio::co_spawn((boost::asio::io_context &)ioc, [this]() -> boost::asio::awaitable<void> {
        return scheduleQueue();
    }, boost::asio::detached);
}

Awaitable<void> Log::Log::scheduleQueue()
{
    /* Keep calling the store coroutine until the queue is empty. New items can be added while we're doing this that
       this loop will handle. */
    while (!queue.empty()) {
        // Store the first item.
        try {
            // Unfortunately, this can't be a move because it could race with operator[].
            co_await store(queue.front());
        }
        catch (const std::exception &e) {
            fprintf(stderr, "Error storing exception: %s\n", e.what());
        }
        catch (...) {
            fprintf(stderr, "Error storing exception.\n");
        }

        // Now that we've done the store, remove the remnant of the queue item. That signals that this method is not
        // ongoing if this leaves the queue empty.
        queue.pop_front();
        writtenItems++;
    }
}
