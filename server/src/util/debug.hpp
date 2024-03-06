#pragma once

#include <cassert>

/**
 * @defgroup debug Debug Tools
 *
 * Stuff for debugging.
 */
/// @addtogroup debug
/// @{

#ifdef NDEBUG
[[noreturn]] inline void unreachable()
{
    __builtin_unreachable();
}
#else // NDEBUG
/**
 * A function that indicates unreachability.
 *
 * This is an assertion for debug builds, and undefined behaviour for release builds.
 */
[[noreturn]] inline void unreachable()
{
    assert(false);
}
#endif // NDEBUG

/// @}
