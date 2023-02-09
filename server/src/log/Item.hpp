#pragma once

#include "Level.hpp"

#include <chrono>
#include <string>
#include <string_view>

namespace Log
{

/**
 * Represents a log item.
 */
struct Item final
{
    /**
     * Parse from a JSON string.
     */
    static Item fromJsonString(std::string_view jsonString);

    /**
     * Encode as a JSON string.
     */
    std::string toJsonString() const;

    /**
     * Format the log item to a string.
     *
     * @param colour Include ANSI escape sequences for terminal colour.
     */
    std::string format(bool colour = false) const;

    /**
     * Time since the Log was created that this log entry was created.
     */
    std::chrono::steady_clock::duration logTime{0};

    /**
     * Time since the Context was created that this log entry was created.
     */
    std::chrono::steady_clock::duration contextTime{0};

    /**
     * Wall clock time that this log entry was created.
     */
    std::chrono::system_clock::time_point systemTime{std::chrono::system_clock::duration{0}};

    /**
     * The log item severity.
     */
    Level level = Level::info;

    /**
     * The message kind.
     *
     * The idea is that the consumer of the log might be interested in specific kinds of messages, and this field gives
     * the consumer a way to differentiate between messages of that kind and other messages from the same context.
     */
    std::string kind;

    /**
     * The logged message.
     */
    std::string message;

    /**
     * The name of the context this item belongs to.
     */
    std::string contextName;

    /**
     * The index within the contexts of the given name that this item belongs to.
     */
    size_t contextIndex = 0;

#ifdef WITH_TESTING
    bool operator==(const Item &) const;
#endif // WITH_TESTING
};

} // namespace Log
