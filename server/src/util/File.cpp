#include "File.hpp"

#include "util/asio.hpp"
#include "util/debug.hpp"
#include "util/util.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/write.hpp>

namespace
{

boost::asio::stream_file::flags getFlags(bool writable, bool readable)
{
    if (writable) {
        return boost::asio::stream_file::create | boost::asio::stream_file::truncate |
               (readable ? boost::asio::stream_file::read_write : boost::asio::stream_file::write_only);
    }
    else if (readable) {
        return boost::asio::stream_file::read_only;
    }
    unreachable();
}

} // namespace

struct Util::File::Stream final
{
    template <typename... Args>
    Stream(Args &&...args) : file(std::forward<Args>(args)...) {}

    boost::asio::stream_file file;
};

Util::File::~File() = default;

Util::File::File() = default;

Util::File::File(IOContext &ioc, std::filesystem::path path, bool writable, bool readable) :
    file(std::make_unique<Stream>(ioc, path, getFlags(writable, readable))), path(std::move(path))
{
}

Util::File &Util::File::operator=(File &&other) = default;

Awaitable<std::vector<std::byte>> Util::File::readSome()
{
    /* Keep trying to read something until we get a non-empty result or end of body. */
    while (true) {
        // Allocate a buffer if one doesn't already exist.
        if (buffer.empty()) {
            buffer = std::vector<std::byte>(1 << 16);
        }

        // Try to read some data from the file.
        auto [e, n] = co_await file->file.async_read_some(boost::asio::buffer(buffer),
                                                          boost::asio::as_tuple(boost::asio::use_awaitable));

        // Return the buffer by moving if most of it was used.
        // The idea is to avoid the copy. If most of it is unused, then it could be inefficient to the entire memory
        // allocation for just a small amount of data.
        if (n >= buffer.size() / 2) {
            buffer.resize(n);
            co_return std::move(buffer);
        }

        // Otherwise, copy the data if there isn't enough for the above optimization to be efficient.
        else if (n > 0) {
            co_return std::vector<std::byte>(buffer.begin(), buffer.begin() + n);
        }

        // Handle end of file by returning empty.
        if (e == boost::asio::error::eof) {
            co_return std::vector<std::byte>();
        }

        // Other errors.
        else if (e) {
            throw std::runtime_error("Error reading file " + (std::string)path + ": " + e.message());
        }
    }
}

Awaitable<std::vector<std::byte>> Util::File::readAll()
{
    std::vector<std::vector<std::byte>> dataParts;
    while (true) {
        std::vector<std::byte> part = co_await readSome();
        if (part.empty()) {
            co_return Util::concatenate(dataParts);
        }
        dataParts.emplace_back(std::move(part));
    }
}

Awaitable<std::vector<std::byte>> Util::File::readExact(size_t length)
{
    std::vector<std::byte> result(length);
    co_await boost::asio::async_read(file->file, boost::asio::mutable_buffer(result.data(), result.size()),
                                      boost::asio::use_awaitable);
    co_return result;
}

Awaitable<void> Util::File::write(std::span<const std::byte> data)
{
    co_await boost::asio::async_write(file->file, boost::asio::const_buffer(data.data(), data.size()),
                                      boost::asio::use_awaitable);
}

void Util::File::seek(size_t offset)
{
    [[maybe_unused]] uint64_t newOffset = file->file.seek(offset, boost::asio::stream_file::seek_basis::seek_set);
    assert(offset == newOffset);
}

void Util::File::seekToEnd()
{
    file->file.seek(0, boost::asio::stream_file::seek_basis::seek_end);
}
