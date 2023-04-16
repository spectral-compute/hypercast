#pragma once

#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

template <typename> class Awaitable;
class IOContext;

/// @addtogroup util
/// @{

/**
 * Tools for running subprocesses.
 */
namespace Subprocess
{

/**
 * Run a process that can be interacted with via its stdin, stdout, and stderr.
 */
class Subprocess final
{
public:
    ~Subprocess();

    /**
     * Start running a subprocess.
     *
     * @param executable The executable to run. This is searched for in PATH.
     * @param arguments The arguments (excluding the executable) to give to the subprocess.
     * @param captureStdin Make it possible to supply data to the subprocess via stdin.
     * @param captureStdout Make it possible to retrieve data from the subprocess's stdout.
     * @param captureStderr Make it possible to retrieve data from the subprocess's stderr.
     */
    explicit Subprocess(IOContext &ioc, std::string_view executable, std::span<const std::string> arguments,
                        bool captureStdin = true, bool captureStdout = true, bool captureStderr = true);

    /**
     * @copydoc Subprocess
     */
    explicit Subprocess(IOContext &ioc, std::string_view executable, std::span<const std::string_view> arguments,
                        bool captureStdin = true, bool captureStdout = true, bool captureStderr = true);

    /**
     * @copydoc Subprocess
     */
    explicit Subprocess(IOContext &ioc, std::string_view executable, std::initializer_list<std::string_view> arguments,
                        bool captureStdin = true, bool captureStdout = true, bool captureStderr = true) :
        Subprocess(ioc, executable, std::span(arguments.begin(), arguments.end()),
                   captureStdin, captureStdout, captureStderr)
    {
    }

    Subprocess(Subprocess &&) = default;

    /**
     * Wait for the process to terminate.
     *
     * @param throwOnNonZero Throw an exception if the return code is non-zero.
     * @return The return code.
     * @throws std::runtime_error If the sub-process returns non-zero unless throwOnNonZero is false.
     */
    Awaitable<int> wait(bool throwOnNonZero = true);

    /**
     * Stop the process via SIGTERM (or equivalent on other operating systems)
     *
     * Use wait to wait for the process to terminate after calling this method.
     */
    void kill();

    /**
     * Write data to the subprocess's stdin.
     *
     * This method requires that captureStdin was set to true when creating the Subprocess object and closeStdin has
     * not yet been called.
     *
     * @param data The data to write to the subprocess's stdin.
     */
    Awaitable<void> writeStdin(std::span<const std::byte> data);

    /**
     * Write a string to the subprocess's stdin.
     *
     * This method requires that captureStdin was set to true when creating the Subprocess object and closeStdin has
     * not yet been called.
     *
     * @param data The string to write to the subprocess's stdin.
     */
    Awaitable<void> writeStdin(std::string_view data);

    /**
     * Close the subprocess's stdin.
     *
     * Some programs use this to know when they've received their full input.
     */
    void closeStdin();

    /**
     * Read data from the subprocess's stdout.
     *
     * This method requires that captureStdout was set to true when creating the Subprocess object and closeStdout has
     * not yet been called.
     *
     * @return Some data from the subprocess's stdout, or empty if its end of file has been reached.
     */
    Awaitable<std::vector<std::byte>> readStdout();

    /**
     * Read a line from the subprocess's stdout.
     *
     * This method requires that captureStdout was set to true when creating the Subprocess object and closeStdout has
     * not yet been called.
     *
     * @return A line from the subprocess's stdout, or std::nullopt if the end of file has been reached. The optional
     *         distinguishes between an empty line and end of file.
     */
    Awaitable<std::optional<std::string>> readStdoutLine();

    /**
     * Read everything (remaining) from stdout.
     *
     * This method requires that captureStdout was set to true when creating the Subprocess object and closeStdout has
     * not yet been called.
     *
     * @warning Care must be taken not to cause a deadlock if stdout and stderr are both captured. A deadlock can be
     *          avoided by, for example: `co_await (subprocess.readAllStdout() && subprocess.readAllStderr())`
     *
     * @return All the (remaining) data the process returns via stdout before closing it.
     */
    Awaitable<std::vector<std::byte>> readAllStdout();

    /**
     * Read everything (remaining) from stdout as a string.
     *
     * This method requires that captureStdout was set to true when creating the Subprocess object and closeStdout has
     * not yet been called.
     *
     * @warning Care must be taken not to cause a deadlock if stdout and stderr are both captured. A deadlock can be
     *          avoided by, for example:
     *          `co_await (subprocess.readAllStdoutAsString() && subprocess.readAllStderrAsString())`
     *
     * @return All the (remaining) data the process returns via stdout before closing it.
     */
    Awaitable<std::string> readAllStdoutAsString();

    /**
     * Close the subprocess's stdout.
     */
    void closeStdout();

    /**
     * Read data from the subprocess's stderr.
     *
     * This method requires that captureStderr was set to true when creating the Subprocess object and closeStderr has
     * not yet been called.
     *
     * @return Some data from the subprocess's stderr, or empty if its end of file has been reached.
     */
    Awaitable<std::vector<std::byte>> readStderr();

    /**
     * Read a line from the subprocess's stderr.
     *
     * This method requires that captureStderr was set to true when creating the Subprocess object and closeStderr has
     * not yet been called.
     *
     * @return A line from the subprocess's stderr, or std::nullopt if the end of file has been reached. The optional
     *         distinguishes between an empty line and end of file.
     */
    Awaitable<std::optional<std::string>> readStderrLine();

    /**
     * Read everything (remaining) from stderr.
     *
     * This method requires that captureStderr was set to true when creating the Subprocess object and closeStderr has
     * not yet been called.
     *
     * @warning Care must be taken not to cause a deadlock if stdout and stderr are both captured. A deadlock can be
     *          avoided by, for example: `co_await (subprocess.readAllStdout() && subprocess.readAllStderr())`
     *
     * @return All the (remaining) data the process returns via stderr before closing it.
     */
    Awaitable<std::vector<std::byte>> readAllStderr();

    /**
     * Read everything (remaining) from stderr as a string.
     *
     * This method requires that captureStderr was set to true when creating the Subprocess object and closeStderr has
     * not yet been called.
     *
     * @warning Care must be taken not to cause a deadlock if stdout and stderr are both captured. A deadlock can be
     *          avoided by, for example:
     *          `co_await (subprocess.readAllStdoutAsString() && subprocess.readAllStderrAsString())`
     *
     * @return All the (remaining) data the process returns via stderr before closing it.
     */
    Awaitable<std::string> readAllStderrAsString();

    /**
     * Close the subprocess's stderr.
     */
    void closeStderr();

private:
    /**
     * A wrapper around the actual subprocess.
     *
     * Having this, and the streams be smart pointers facilitates making this object movable and thus able to be
     * returned from a function. It also allows us to avoid including hundreds of thousands lines of stuff from Boost
     * headers and in the case of the streams, is a bit more modular.
     */
    struct Process;

    /**
     * A pipe for input to the process, such as stdin.
     */
    struct InPipe;

    /**
     * A pipe for output from the process, such as stdout.
     */
    struct OutPipe;

    /**
     * An object that represents the sub-process itself.
     */
    std::unique_ptr<Process> process;

    /**
     * If non-null, stdin.
     *
     * If null, then not captured.
     */
    std::unique_ptr<InPipe> stdinPipe;

    /**
     * If non-null, stdout.
     *
     * If null, then not captured.
     */
    std::unique_ptr<OutPipe> stdoutPipe;

    /**
     * If non-null, stderr.
     *
     * If null, then not captured.
     */
    std::unique_ptr<OutPipe> stderrPipe;
};

/**
 * Run a subprocess and return its stdout.
 *
 * @param executable The executable to run. This is searched for in PATH.
 * @param arguments The arguments (excluding the executable) to give to the subprocess.
 * @return What the subprocess wrote to stdout.
 * @throws std::runtime_error If the sub-process returns non-zero.
 */
Awaitable<std::string> getStdout(IOContext &ioc, std::string_view executable,
                                 std::span<const std::string_view> arguments = {});

/**
 * @copydoc getStdout
 */
Awaitable<std::string> getStdout(IOContext &ioc, std::string_view executable,
                                 std::initializer_list<std::string_view> arguments = {});

} // namespace Subprocess

/// @}
