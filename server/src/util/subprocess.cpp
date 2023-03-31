#include "subprocess.hpp"

#include "util/asio.hpp"
#include "util/debug.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>

#include <boost/process/v2/environment.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>

#include <sys/prctl.h>

#include <algorithm>
#include <stdexcept>

struct Subprocess::Subprocess::InPipe final
{
    explicit InPipe(IOContext &ioc) : pipe(ioc) {}
    boost::asio::writable_pipe pipe;
};

struct Subprocess::Subprocess::OutPipe final
{
    explicit OutPipe(IOContext &ioc) : pipe(ioc) {}

    /**
     * Read some data from the pipe, dealing with the remainder from readLine if necessary.
     *
     * @return Some data from the subprocess's stdout, or empty if its end of file has been reached.
     */
    Awaitable<std::vector<std::byte>> read()
    {
        /* If we had remaining data from a previous readLine, then return that (and remove it from remainder). */
        if (!remainder.empty()) {
            // Returning immediately, and not attempting to read more, avoids a potential delay.
            co_return std::move(remainder);
        }

        /* Try to return some data. */
        try {
            std::byte buffer[4096];
            size_t n = co_await pipe.async_read_some(boost::asio::buffer(buffer), boost::asio::use_awaitable);
            co_return std::vector<std::byte>(buffer, buffer + n);
        }
        catch (const boost::system::system_error &e) {
            if (e.code() == boost::asio::error::eof) {
                // End of file is not an error.
                co_return std::vector<std::byte>();
            }
            throw;
        }
    }

    /**
     * Read a line from the pipe, dealing with or using the remainder from readLine if necessary.
     *
     * @return A line from the subprocess's stdout, or std::nullopt if the end of file has been reached. The optional
     *         distinguishes between an empty line and end of file.
     */
    Awaitable<std::optional<std::string>> readLine()
    {
        std::string result;
        for (bool first = true; ; first = false) {
            /* Read some more data. */
            std::vector<std::byte> data = co_await read();
            assert(remainder.empty()); // The above read should have returned remainder, if any.

            /* Handle end-of-file. */
            if (data.empty()) {
                co_return first ? std::nullopt : std::optional(result);
            }

            /* Find where the line ends, and stash the rest away in . */
            auto it = std::find_if(data.begin(), data.end(), [](std::byte b) {
                return b == (std::byte)'\n' || b == (std::byte)'\r';
            });

            /* Handle the case where no newline is found. */
            if (it == data.end()) {
                // Add the entire partial line to the result.
                result.insert(result.end(), (const char *)data.data(), (const char *)data.data() + data.size());

                // Read some more data.
                continue;
            }

            /* Stash everything after the newline in remainder. */
            bool isTwoCharNewline = it < data.end() - 1 && *it == (std::byte)'\r' && *(it + 1) == (std::byte)'\n';
            remainder.insert(remainder.end(), it + (isTwoCharNewline ? 2 : 1), data.end());

            /* Return the result, including everything up to but excluding the newline. */
            result.insert(result.end(), (const char *)data.data(), (const char *)&*it);
            co_return result;
        }
    }

    /**
     * Read everything until end-of-file.
     */
    Awaitable<std::vector<std::byte>> readAll()
    {
        std::vector<std::byte> result = std::move(remainder); // Optimization, since read would do this too.
        while (true) {
            std::vector<std::byte> data = co_await read();
            if (data.empty()) {
                co_return result;
            }
            result.insert(result.end(), data.begin(), data.end());
        }
    }

    /**
     * Read everything until end-of-file as a string.
     */
    Awaitable<std::string> readAllAsString()
    {
        std::vector<std::byte> data = co_await readAll();
        co_return std::string((const char *)data.data(), data.size());
    }

    boost::asio::readable_pipe pipe;

private:
    std::vector<std::byte> remainder;
};

namespace
{

/**
 * A Boost Process initializer to make the subprocess terminate if the parent does.
 *
 * The default, at least on Linux, is for orphaned processes to be adopted by init. Unfortunately, that means that if
 * the server crashes, long running processes (like ffmpeg) keep going, which we don't want.
 */
struct KillOnDetachProcessInitializer final
{
    /*KillOnDetachProcessInitializer()
    {
        for (int fd = 3; fd < 1024; fd++) {
            int flags = fcntl(fd, F_GETFD);
            if (flags < 0) {
                continue;
            }
            flags &= ~FD_CLOEXEC;
            if (fcntl(fd, F_SETFD, flags)) {
                throw std::runtime_error("Error removing FD_CLOEXEC from file descriptor" + std::to_string(fd) + ": " +
                                         strerror(errno));
            }
        }
    }*/

    template <typename... Args>
    boost::process::v2::error_code on_exec_setup(Args &&...)
    {
        if (int e = prctl(PR_SET_PDEATHSIG, SIGKILL)) {
            return boost::system::error_code(e, boost::system::system_category());
        }
        return {};
    }
};

} // namespace

struct Subprocess::Subprocess::Process final
{
    explicit Process(IOContext &ioc, std::string_view executable, const std::span<const std::string> &arguments,
                     boost::asio::writable_pipe *stdinPipe, boost::asio::readable_pipe *stdoutPipe,
                     boost::asio::readable_pipe *stderrPipe) :
        process(ioc, boost::process::v2::environment::find_executable(executable), arguments,
                getStdio(stdinPipe, stdoutPipe, stderrPipe), KillOnDetachProcessInitializer{})
    {
    }

    boost::process::v2::process process;

private:
    /**
     * Unfortunately, boost::process::v2::process_stdio has fields that are of unspecified type, which means they can't
     * easily be made conditional in the way we'd like.
     */
    static boost::process::v2::process_stdio getStdio(boost::asio::writable_pipe *stdinPipe,
                                                      boost::asio::readable_pipe *stdoutPipe,
                                                      boost::asio::readable_pipe *stderrPipe)
    {
        if (!stdinPipe && !stdoutPipe && !stderrPipe) {
            return {nullptr, nullptr, nullptr};
        }
        if (!stdinPipe && !stdoutPipe && stderrPipe) {
            return {nullptr, nullptr, *stderrPipe};
        }
        if (!stdinPipe && stdoutPipe && !stderrPipe) {
            return {nullptr, *stdoutPipe, nullptr};
        }
        if (!stdinPipe && stdoutPipe && stderrPipe) {
            return {nullptr, *stdoutPipe, *stderrPipe};
        }
        if (stdinPipe && !stdoutPipe && !stderrPipe) {
            return {*stdinPipe, nullptr, nullptr};
        }
        if (stdinPipe && !stdoutPipe && stderrPipe) {
            return {*stdinPipe, nullptr, *stderrPipe};
        }
        if (stdinPipe && stdoutPipe && !stderrPipe) {
            return {*stdinPipe, *stdoutPipe, nullptr};
        }
        if (stdinPipe && stdoutPipe && stderrPipe) {
            return {*stdinPipe, *stdoutPipe, *stderrPipe};
        }
        unreachable();
    }
};

namespace
{

/**
 * Convert a span of string views to a vector of strings.
 *
 * For some reason, boost::process::v2::process's constructor doesn't accept std::string_view:
 * no viable conversion from 'const std::basic_string_view<char>' to 'basic_string_view<char_type>'
 */
std::vector<std::string> convertStringViewsToStrings(std::span<const std::string_view> views)
{
    std::vector<std::string> strings;
    strings.reserve(views.size());
    for (std::string_view arg: views) {
        strings.emplace_back(arg);
    }
    return strings;
}

} // namespace

Subprocess::Subprocess::~Subprocess() = default;

Subprocess::Subprocess::Subprocess(IOContext &ioc, std::string_view executable, std::span<const std::string> arguments,
                                   bool captureStdin, bool captureStdout, bool captureStderr) :
    stdinPipe(captureStdin ? std::make_unique<InPipe>(ioc) : nullptr),
    stdoutPipe(captureStdout ? std::make_unique<OutPipe>(ioc) : nullptr),
    stderrPipe(captureStderr ? std::make_unique<OutPipe>(ioc) : nullptr)
{
    process = std::make_unique<Process>(ioc, executable, arguments, stdinPipe ? &stdinPipe->pipe : nullptr,
                                        stdoutPipe ? &stdoutPipe->pipe : nullptr,
                                        stderrPipe ? &stderrPipe->pipe : nullptr);
}

Subprocess::Subprocess::Subprocess(IOContext &ioc, std::string_view executable,
                                   std::span<const std::string_view> arguments,
                                   bool captureStdin, bool captureStdout, bool captureStderr) :
    Subprocess(ioc, executable, std::span(convertStringViewsToStrings(arguments).data(), arguments.size()),
               captureStdin, captureStdout, captureStderr)
{
}

Awaitable<int> Subprocess::Subprocess::wait(bool throwOnNonZero)
{
    /* Wait until the process is terminated. */
    co_await process->process.async_wait(boost::asio::use_awaitable);

    /* Return the return code. */
    int retCode = process->process.exit_code();
    if (throwOnNonZero && retCode != 0) {
        throw std::runtime_error("Subprocess returned " + std::to_string(retCode) + ".");
    }
    co_return retCode;
}

Awaitable<void> Subprocess::Subprocess::writeStdin(std::span<const std::byte> data)
{
    assert(stdinPipe);
    co_await stdinPipe->pipe.async_write_some(boost::asio::const_buffer(data.data(), data.size()),
                                              boost::asio::use_awaitable);
}

Awaitable<void> Subprocess::Subprocess::writeStdin(std::string_view data)
{
    return writeStdin(std::span((const std::byte *)data.data(), data.size()));
}

void Subprocess::Subprocess::closeStdin()
{
    assert(stdinPipe);
    stdinPipe->pipe.close();
    stdinPipe.reset(); // Pipe is no longer writable.
}

Awaitable<std::vector<std::byte>> Subprocess::Subprocess::readStdout()
{
    assert(stdoutPipe);
    return stdoutPipe->read();
}

Awaitable<std::optional<std::string>> Subprocess::Subprocess::readStdoutLine()
{
    assert(stdoutPipe);
    return stdoutPipe->readLine();
}

Awaitable<std::vector<std::byte>> Subprocess::Subprocess::readAllStdout()
{
    assert(stdoutPipe);
    return stdoutPipe->readAll();
}

Awaitable<std::string> Subprocess::Subprocess::readAllStdoutAsString()
{
    assert(stdoutPipe);
    return stdoutPipe->readAllAsString();
}

void Subprocess::Subprocess::closeStdout()
{
    assert(stdoutPipe);
    stdoutPipe->pipe.close();
    stdoutPipe.reset(); // Pipe is no longer readable.
}

Awaitable<std::vector<std::byte>> Subprocess::Subprocess::readStderr()
{
    assert(stderrPipe);
    return stderrPipe->read();
}

Awaitable<std::optional<std::string>> Subprocess::Subprocess::readStderrLine()
{
    assert(stderrPipe);
    return stderrPipe->readLine();
}

Awaitable<std::vector<std::byte>> Subprocess::Subprocess::readAllStderr()
{
    assert(stderrPipe);
    return stderrPipe->readAll();
}

Awaitable<std::string> Subprocess::Subprocess::readAllStderrAsString()
{
    assert(stderrPipe);
    return stderrPipe->readAllAsString();
}

void Subprocess::Subprocess::closeStderr()
{
    assert(stderrPipe);
    stderrPipe->pipe.close();
    stderrPipe.reset(); // Pipe is no longer readable.
}

Awaitable<std::string> Subprocess::getStdout(IOContext &ioc, std::string_view executable,
                                             std::span<const std::string_view> arguments)
{
    /* Start the subprocess. */
    Subprocess subprocess(ioc, executable, arguments, false);

    /* Read stdout and stderr. */
    auto [stdoutString, stderrString] =
        co_await (subprocess.readAllStdoutAsString() && subprocess.readAllStderrAsString());

    /* Handle errors. */
    int retCode = co_await subprocess.wait(false);
    if (retCode != 0) {
        throw std::runtime_error("Subprocess " + std::string(executable) + " returned " + std::to_string(retCode) +
                                 ", and stderr:\n" + stderrString);
    }

    /* Return stdout. */
    co_return stdoutString;
}

Awaitable<std::string> Subprocess::getStdout(IOContext &ioc, std::string_view executable,
                                             std::initializer_list<std::string_view> arguments)
{
    return getStdout(ioc, executable, std::span(arguments.begin(), arguments.end()));
}
