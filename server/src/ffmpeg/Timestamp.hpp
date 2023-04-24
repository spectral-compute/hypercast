#pragma once

#include <cassert>
#include <compare>
#include <cstdint>
#include <numeric>
#include <utility>

namespace Ffmpeg
{

/**
 * Represents a timestamp (e.g: a presentation timestamp or a decode timestamp).
 *
 * Timestamps in many media formats, and in FFmpeg, are represented as an integer value multiplied by some rational
 * period of time in seconds. This class represents timestamps the same way.
 */
class Timestamp final
{
public:
    /**
     * Create a null timestamp.
     */
    Timestamp() = default;

    /**
     * Create a timestamp with a specific value.
     *
     * The resulting object represents a period of `value * timeBaseNumerator / timeBaseDenominator` seconds.
     *
     * @param value The integer value of the timestamp.
     * @param timeBaseNumerator The numerator of the time base (in seconds).
     * @param timeBaseDenominator The denominator of the time base (in seconds).
     */
    Timestamp(int64_t value, int64_t timeBaseNumerator, int64_t timeBaseDenominator) :
        value(value), timeBaseNumerator(timeBaseNumerator), timeBaseDenominator(timeBaseDenominator)
    {
        assert(timeBaseNumerator != 0);
        assert(timeBaseDenominator != 0);
    }

    /**
     * Tell if the timestamp is initialized.
     *
     * @return True if the timestamp represents a time, and false if it's null (which the default constructor produces).
     */
    operator bool() const
    {
        return timeBaseDenominator;
    }

    /**
     * Order timestamps in time.
     *
     * Null timestamps are ordererd before all non-null timestamps.
     */
    std::weak_ordering operator<=>(const Timestamp &rhs) const
    {
        /* Handle null timestamps. */
        // If both are null, then their denominators will be the same, and their values and numerators zero, so the
        // comparison for equal denominator will return equal.
        if (*this && !rhs) {
            return std::weak_ordering::greater;
        }
        if (!*this && rhs) {
            return std::weak_ordering::less;
        }

        /* If the time baes denominators are the same, then pairwise multiplication should work. */
        if (rhs.timeBaseDenominator == timeBaseDenominator) {
            // Further integer overflow avoidance could be achieved by finding the GCD of the numerators, but that's
            // probably unnecessary.
            return value * timeBaseNumerator <=> rhs.value * rhs.timeBaseNumerator;
        }

        /* Convert the timestamps to have a common denominator. */
        auto [commonLhs, commonRhs] = getCommonDenominatorTimestamps(*this, rhs);
        return commonLhs <=> commonRhs;
    }

    bool operator==(const Timestamp &rhs) const
    {
        return (*this <=> rhs) == std::weak_ordering::equivalent;
    }

    /**
     * Get the integer value of the timestamp.
     *
     * This can be multiplied by the rational number returned by getTimeBase to get a time in seconds.
     *
     * @return The timestamp, measured in units of `getTimeBase().first / getTimeBase().second` seconds.
     */
    int64_t getValue() const
    {
        return value;
    }

    /**
     * Get the time base of the timestamp.
     *
     * This is a rational time increment, in seconds, that provides units that the integer returned by getValue is
     * measured in.
     *
     * @return A `{ numerator, denominator }` pair that specifies the units of the timestamp value in seconds.
     */
    std::pair<int64_t, int64_t> getTimeBase() const
    {
        return { timeBaseNumerator, timeBaseDenominator };
    }

    /**
     * Get the timestamp in seconds.
     */
    double getValueInSeconds() const
    {
        return (double)value * (double)timeBaseNumerator / (double)timeBaseDenominator;
    }

private:
    /**
     * Convert this timestamp and another to a time base representation that has a common denominator between both
     * timestamps.
     *
     * @param lhs The timestamp to use as the first timestamp in the result.
     * @param rhs The timestamp to use as the second timestamp in the result.
     * @return A pair of converted timestamps for: { lhs, rhs }.
     */
    static std::pair<Timestamp, Timestamp> getCommonDenominatorTimestamps(const Timestamp &lhs, const Timestamp &rhs)
    {
        int64_t lcm = std::lcm(lhs.timeBaseDenominator, rhs.timeBaseDenominator);
        return {
            { lhs.value, lhs.timeBaseNumerator * (lcm / lhs.timeBaseDenominator ), lcm },
            { rhs.value, rhs.timeBaseNumerator * (lcm / rhs.timeBaseDenominator ), lcm }
        };
    }

    int64_t value = 0;
    int64_t timeBaseNumerator = 0;
    int64_t timeBaseDenominator = 0;
};

} // namespace Ffmpeg
