#include "subprocess.hpp"

#include "util/asio.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/readable_pipe.hpp>

#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>

#include <stdexcept>

namespace
{

/**
 * Read a pipe (i.e: stdout or stderr) to a string.
 */
Awaitable<std::string> readPipe(boost::asio::readable_pipe &pipe)
{
    std::string result;
    while (true) {
        char buffer[4096];
        auto [e, n] = co_await pipe.async_read_some(boost::asio::buffer(buffer),
                                                    boost::asio::as_tuple(boost::asio::use_awaitable));
        result += std::string_view(buffer, n);

        if (e == boost::asio::error::eof) {
            co_return result;
        }
        else if (e) {
            throw std::runtime_error("Error reading pipe.");
        }
    }
}

} // namespace

Awaitable<std::string> Subprocess::getStdout(IOContext &ioc, std::string_view executable,
                                             std::span<const std::string_view> arguments)
{
    /* For some reason, boost::process::v2::process's constructor doesn't accept std::string_view:
       no viable conversion from 'const std::basic_string_view<char>' to 'basic_string_view<char_type>' */
    std::vector<std::string> argumentStrings;
    argumentStrings.reserve(arguments.size());
    for (std::string_view arg: arguments) {
        argumentStrings.emplace_back(arg);
    }

    /* Create the process and pipes. */
    boost::asio::readable_pipe stdoutPipe(ioc);
    boost::asio::readable_pipe stderrPipe(ioc);
    boost::process::v2::process process(ioc, boost::process::v2::environment::find_executable(executable),
                                        argumentStrings,
                                        boost::process::v2::process_stdio{nullptr, stdoutPipe, stderrPipe});

    /* Wait for the process to terminate, and get its output. */
    auto [_, stdoutString, stderrString] = co_await (process.async_wait(boost::asio::use_awaitable) &&
                                                     readPipe(stdoutPipe) && readPipe(stderrPipe));
    int retCode = process.exit_code();

    /* Handle errors. */
    if (retCode != 0) {
        throw std::runtime_error("Subprocess " + std::string(executable) + " returned " + std::to_string(retCode) +
                                 ", and stderr:\n" + stderrString);
    }

    /* Return stdout. */
    co_return stdoutString;
}
