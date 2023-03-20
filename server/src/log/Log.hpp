#pragma once

#include "Item.hpp"
#include "util/Event.hpp"

#include <deque>
#include <map>
#include <sstream>

template <typename> class Awaitable;
class IOContext;

/**
 * @defgroup log Logging
 *
 * The logging system.
 */
/// @addtogroup log
/// @{

/**
 * Logging related stuff.
 *
 * The idea is to build a log system that's easy to use, but also makes it easy to build an HTTP-based API that can
 * fetch data from the log.
 */
namespace Log
{

class Log;

/**
 * A logging contexts.
 *
 * This is meant so that things like ffmpeg output can be logged separately to things like exceptions from handling HTTP
 * requests, which are useful to inspect with separation between each request.
 *
 * The operator<< method creates a single log entry, and returns a stream to fill in the log entry's message. Thus a
 * log entry can be created with, e.g: context << Level::warning << "Don't delete cats. They're too cute.";.
 */
class Context final
{
private:
    /**
     * A stream that represents a single new log item that's being written.
     */
    class NewItem final
    {
    public:
        /**
         * Write the log item.
         */
        ~NewItem();

        // No copying or moving.
        NewItem(const NewItem &) = delete;
        NewItem(NewItem &&) = delete;
        NewItem &operator=(const NewItem &) = delete;
        NewItem &operator=(NewItem &&) = delete;

        /**
         * Append a formatted object to the new log item's message.
         */
        template <typename T>
        NewItem &operator<<(T &&value)
        {
            stream << std::forward<T>(value);
            return *this;
        }

    private:
        friend class Context;

        /**
         * Create a log item to write to now.
         */
        explicit NewItem(Context &parent, Level level, std::string_view kind);

        const std::chrono::steady_clock::time_point steadyCreationTime;
        const std::chrono::system_clock::time_point systemCreationTime;

        Context &parent;
        const Level level;
        std::string kind; // Non-const so the destructor can move it.

        /**
         * The stream to write to.
         */
        std::stringstream stream;
    };

    /**
     * Object to allow context << "kind" << level << "message";
     */
    struct ReverseItemInfo final
    {
        NewItem operator<<(Level level)
        {
            return NewItem(context, level, kind);
        }

        Context &context;
        std::string_view kind;
    };

public:
    /**
     * Describes the log entry to be created with operator<<.
     */
    struct ItemInfo final
    {
        /**
         * Constructor :)
         *
         * @param level The log severity for the new log item.
         * @param kind The value to give Item::kind.
         */
        ItemInfo(Level level = Level::info, std::string_view kind = "") : level(level), kind(kind) {}
        Level level;
        std::string_view kind;
    };

    /**
     * Destroy the context and record its destruction.
     */
    ~Context();

    // No copying or moving.
    Context(const Context &) = delete;
    Context(Context &&) = delete;
    Context &operator=(const Context &) = delete;
    Context &operator=(Context &&) = delete;

    /**
     * Create a log entry.
     *
     * Allows:
     *
     *     context << Level::info << "message";
     *     context << ItemInfo(Level::info, "kind") << "message";
     *
     * Unfortunately, brace initialization of ItemInfo isn't permitted.
     *
     * @param info Describes the log entry to be created. This can also be given a Level, and it'll implicitly convert.
     * @return A stream to write the log entry to. This returns by value so that when it goes out of scope, the log
     *         entry can be written to the log.
     */
    NewItem operator<<(ItemInfo info)
    {
        return NewItem(*this, info.level, info.kind);
    }

    /**
     * Create a log entry.
     *
     * Allows:
     *
     *     context << "kind" << Level::info << "message";
     *
     * This overload exists to provide a nicer alternative syntax to:
     *
     *     context << Context::ItemInfo(Level::info, "kind") << "message";
     *
     * @param kind The value to give Item::kind.
     * @return An object whose operator<< accepts a Level and produces an object into which the log item's message can
     *         be streamed.
     */
    ReverseItemInfo operator<<(std::string_view kind)
    {
        return {*this, kind};
    }

private:
    friend class Log; // Only Log should be able to create Context objects.
    friend class NewItem; // Only NewItem should call append.

    /**
     * Create the context and record its creation.
     *
     * @param parent The Log that this context belongs to.
     * @param name The name of the context. It is valid for more than one context to be given the same name. Multiple
     *             such contexts are distinguished by an automatically assigned index.
     * @param index The index of this context within those contexts with the same name.
     */
    explicit Context(Log &parent, std::string name, size_t index);

    /**
     * Append the given item to the log.
     */
    void append(NewItem &newItem);

    const std::chrono::steady_clock::time_point steadyCreationTime;

    Log &parent;
    std::string name; // Non-const so the destructor can move it.
    const size_t index;
};

/**
 * Manages a log.
 *
 * The log is maintained in such a way that it can be accessed by the application as well. This is useful to be able to
 * build an HTTP API that gives access to information from the log.
 *
 * This is an abstract class for better testing, to abstract away and separate the Boost filesystem and asio,
 * implementation, and to make it clearer to have different implementations (e.g: an in-memory implementation).
 */
class Log
{
public:
    /**
     * Finish the log and record this.
     */
    virtual ~Log();

    // No copying or moving.
    Log(const Log &) = delete;
    Log(Log &&) = delete;
    Log &operator=(const Log &) = delete;
    Log &operator=(Log &&) = delete;

    /**
     * Create a new context with the given name.
     */
    Context operator()(std::string_view name);

    /**
     * Get the log entry with the given
     */
    Awaitable<Item> operator[](size_t index) const;

    /**
     * Get the number of log entries.
     */
    size_t size() const
    {
        return writtenItems + queue.size();
    }

    /**
     * Wait for a new log entry to be added to the log.
     */
    Awaitable<void> wait() const;

protected:
    /**
     * Create a log and record this.
     *
     * @param minLevel The minimum level to log.
     * @param print Print the log to stderr.
     */
    explicit Log(Level minLevel, bool print, IOContext &ioc);

    /**
     * Get the number of times store has been called.
     *
     * If called during store, this excludes the item being stored.
     */
    size_t getWrittenItemCount() const
    {
        return writtenItems;
    }

    IOContext &ioc;

private:
    friend class Context; // Only Context should call append.

    /**
     * Load an item from the stored log.
     * @param index The index of the item to load.
     */
    virtual Awaitable<Item> load(size_t index) const = 0;

    /**
     * Add an item to the stored log.
     *
     * This method will not be called in parallel.
     */
    virtual Awaitable<void> store(Item item) = 0;

    /**
     * Add an item to the log.
     */
    void append(Item item);

    /**
     * Schedule the append for ordered asynchronous append.
     */
    void scheduleAppend(Item item);

    /**
     * Schedule the storage of everything in the queue (including everything that's added after the call, but prior to
     * its return).
     */
    Awaitable<void> scheduleQueue();

    const std::chrono::steady_clock::time_point steadyCreationTime;
    const std::chrono::system_clock::time_point systemCreationTime;

    const Level minLevel;
    const bool print;

    /**
     * The number of items that have been written.
     */
    size_t writtenItems = 0;

    /**
     * Map from context name to next index.
     */
    std::map<std::string, size_t> contextNextIndices;

    /**
     * A queue of log items that haven't yet been written.
     */
    std::deque<Item> queue;

    /**
     * An event that's triggered when new items are added to the log.
     */
    Event event;
};

} // namespace Log

/// @}
