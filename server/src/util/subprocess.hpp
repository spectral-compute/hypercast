#pragma once

#include "util/asio.hpp"

#include <initializer_list>
#include <span>
#include <string_view>

/// @addtogroup util
/// @{

/**
 * Tools for running subprocesses.
 */
namespace Subprocess
{

/**
 * Run a subprocess and return its stdout.
 *
 * @param executable The executable to run. This is searched for in PATH.
 * @param arguments The arguments (excluding the executable) to give to the subprocess.
 * @return What the subprocess wrote to stdout.
 * @throws std::runtime_error If the sub-process returns non-zero.
 */
Awaitable<std::string> getStdout(IOContext &ioc, std::string_view executable,
                                 std::span<const std::string_view> arguments);
inline Awaitable<std::string> getStdout(IOContext &ioc, std::string_view executable,
                                        std::initializer_list<std::string_view> arguments)
{
    return getStdout(ioc, executable, std::span(arguments.begin(), arguments.end()));
}

} // namespace Util

/// @}
