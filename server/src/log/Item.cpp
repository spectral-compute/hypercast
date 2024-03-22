#include "Item.hpp"

#include "util/debug.hpp"
#include "util/json.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string_view>

using namespace std::string_literals;

/// @addtogroup log
/// @{
/// @defgroup log_implementation Implementation
/// @}

/// @addtogroup log_implementation
/// @{

namespace
{

/**
 * Convert a log level to a human readable name.
 */
const char *levelToJson(Log::Level level)
{
    switch (level) {
        case Log::Level::debug: return "debug";
        case Log::Level::info: return "info";
        case Log::Level::warning: return "warning";
        case Log::Level::error: return "error";
        case Log::Level::fatal: return "fatal";
    }
    unreachable();
}

/**
 * Convert a log level to a human readable name.
 */
const char *levelToName(Log::Level level)
{
    switch (level) {
        case Log::Level::debug: return "Debug";
        case Log::Level::info: return "Info";
        case Log::Level::warning: return "Warning";
        case Log::Level::error: return "Error";
        case Log::Level::fatal: return "Fatal";
    }
    unreachable();
}

/**
 * Convert a log level to an ANSI escape code colour.
 */
const char *levelToColour(Log::Level level)
{
    switch (level) {
        case Log::Level::debug: return "37;1";
        case Log::Level::info: return "32;1";
        case Log::Level::warning: return "33;1";
        case Log::Level::error: return "31;1";
        case Log::Level::fatal: return "31";
    }
    unreachable();
}

/**
 * Convert a steady clock duration to a formatted timestamp.
 */
std::string convertTime(std::chrono::steady_clock::duration d)
{
    char buffer[512]; // Double precision exponent is around 10^308.
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
    snprintf(buffer, sizeof(buffer), "%0.06f s", ns.count() / 1e9);
    return buffer;
}

/**
 * Convert a system clock time point to a formatted timestamp.
 */
std::string convertTime(std::chrono::system_clock::time_point tp)
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    const std::tm *tm = std::gmtime(&t);

    if (!tm) {
        return "unknown";
    }

    std::stringstream result;
    result << std::put_time(tm, "%F %T");
    return result.str();
}

/**
 * Conditionally converts an ANSI escape code sequence (https://en.wikipedia.org/wiki/ANSI_escape_code) to a string.
 *
 * The condition is given at the constructor.
 */
class Colour final
{
public:
    explicit Colour(bool colour) : colour(colour) {}

    /**
     * Perform the conditional conversion.
     */
    std::string operator()(const char *sequence = "") const
    {
        return colour ? "\x1b["s + sequence + "m" : "";
    }

private:
    bool colour;
};

using LogDuration = std::chrono::microseconds;

} // namespace

/// @}

namespace Log
{

static void from_json(const nlohmann::json &j, Item &out)
{
    Json::ObjectDeserializer d(j);

    // Annoyingly, we can't make from_json and to_json out of these, because their type is in the std namespace, and
    // adding to that is undefined behaviour.
    LogDuration::rep logTimeRep = 0;
    LogDuration::rep contextTimeRep = 0;
    LogDuration::rep systemTimeRep = 0;
    d(logTimeRep, "logTime");
    d(contextTimeRep, "contextTime");
    d(systemTimeRep, "systemTime");
    LogDuration logTimeDuration(logTimeRep);
    LogDuration systemTimeDuration(systemTimeRep);
    LogDuration contextTimeDuration(contextTimeRep);
    out.logTime = std::chrono::duration_cast<std::chrono::steady_clock::duration>(logTimeDuration);
    out.systemTime = std::chrono::system_clock::time_point
                     (std::chrono::duration_cast<std::chrono::system_clock::duration>(systemTimeDuration));
    out.contextTime = std::chrono::duration_cast<std::chrono::steady_clock::duration>(contextTimeDuration);

    d(out.level, "level", {
        { Log::Level::debug, "debug" },
        { Log::Level::info, "info" },
        { Log::Level::warning, "warning" },
        { Log::Level::error, "error" },
        { Log::Level::fatal, "fatal" }
    });
    d(out.kind, "kind", false);
    d(out.message, "message");
    d(out.contextName, "contextName");
    d(out.contextIndex, "contextIndex");
    d();
}

static void to_json(nlohmann::json &j, const Item &in)
{
    j = {
            { "logTime", std::chrono::duration_cast<LogDuration>(in.logTime).count() },
            { "contextTime", std::chrono::duration_cast<LogDuration>(in.contextTime).count() },
            // The std::chrono::system_clock epoch is defined as the Unix epoch since C++20.
            { "systemTime", std::chrono::duration_cast<LogDuration>(in.systemTime.time_since_epoch()).count() },
            { "level", levelToJson(in.level) },
            { "message", in.message },
            { "contextName", in.contextName },
            { "contextIndex", in.contextIndex }
        };
    if (!in.kind.empty()) {
        j["kind"] = in.kind;
    }
}

} // namespace Log

Log::Item Log::Item::fromJsonString(std::string_view jsonString)
{
    return Json::parse(jsonString).get<Log::Item>();
}

std::string Log::Item::toJsonString() const
{
    return Json::dump(*this);
}

std::string Log::Item::format(bool colour) const
{
    constexpr const char *timeCol = "34";
    constexpr const char *ctxCol = "36;1";
    constexpr const char *kindCol = "35;1";

    Colour c(colour);
    std::stringstream result;

    result
        // [Level]
        << "[" << c(levelToColour(level)) << levelToName(level) << c()

        // @ logTime = contextName[contextIndex] + contextTime = systemTime
        << "] @ " << c(timeCol) << convertTime(logTime) << c()
        << " = " << c(ctxCol) << contextName << c() << "[" << c(ctxCol) << contextIndex << c() << "]"
        << " + " << c(timeCol) << convertTime(contextTime) << c()
        << " = " << c(timeCol) << convertTime(systemTime) << c()

        // : [kind] message
        << ": [" << c(kindCol) << kind << c() << "] " << message;

    return result.str();
}
