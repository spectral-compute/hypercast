#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>
#include "util/awaitable.hpp"

class IOContext;

namespace Util
{

/**
 * A simple file class.
 */
class File final
{
public:
    ~File();

    /**
     * Create a file object with no open file.
     */
    File();

    /**
     * Open a file.
     *
     * @param path The path of the file to open.
     * @param writable Whether the file should be writable. If set, the file is truncated. Must be true if readable is
     *                 false.
     * @param readable Whether the file should be readable. Must be true if writable is false.
     */
    explicit File(IOContext &ioc, std::filesystem::path path, bool writable = false, bool readable = true);

    File(File &&) = default;
    File &operator=(File &&other);

    /**
     * Determine whether the file is open.
     */
    operator bool() const
    {
        return (bool)file;
    }

    /**
     * Read some data from the file.
     *
     * @return The read data, or empty if end of file.
     */
    Awaitable<std::vector<std::byte>> readSome();

    /**
     * Read all (remaining) data from the file.
     *
     * @return The (remaining) contents of the file.
     */
    Awaitable<std::vector<std::byte>> readAll();

    /**
     * Read a specific length of data from the file.
     *
     * @param length The amount of data to read.
     * @return The read data, or empty if end of file.
     */
    Awaitable<std::vector<std::byte>> readExact(size_t length);

    /**
     * Write some data to the file.
     */
    Awaitable<void> write(std::span<const std::byte> data);

    /**
     * @copydoc write
     */
    Awaitable<void> write(std::string_view data);

    /**
     * Seek to a given offset in the file.
     *
     * @param offset The offset from the start of the file.
     */
    void seek(size_t offset);

    /**
     * Seek to the end of the file.
     */
    void seekToEnd();

private:
    struct Stream;

    /**
     * An object that represents the file internally.
     *
     * If not set, then the file is not open.
     */
    std::unique_ptr<Stream> file;

    /**
     * The path of the file.
     */
    std::filesystem::path path;

    /**
     * An internal buffer used for efficiency.
     *
     * If the read chunk is large enough, it'll be returned in this buffer as an optimization.
     */
    std::vector<std::byte> buffer;
};

} // namespace Util
