#include "File.hpp"

#include "util/asio.hpp"
#include "util/debug.hpp"
#include "util/util.hpp"

#ifdef BOOST_ASIO_HAS_IO_URING
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/write.hpp>
#else // BOOST_ASIO_HAS_IO_URING
#include <fstream>
#endif // BOOST_ASIO_HAS_IO_URING

namespace
{

#ifdef BOOST_ASIO_HAS_IO_URING
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
#endif // BOOST_ASIO_HAS_IO_URING

} // namespace

struct Util::File::Stream final
{
#ifdef BOOST_ASIO_HAS_IO_URING
    template <typename... Args>
    Stream(Args &&...args) : file(std::forward<Args>(args)...) {}

    boost::asio::stream_file file;
#else // BOOST_ASIO_HAS_IO_URING
    Stream(const std::filesystem::path &path, bool readable, bool writable)
    {
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file.open(path, std::ios::ate | std::ios::binary | (readable ? std::ios::in : (std::ios::openmode)0) |
                        (writable ? std::ios::out | std::ios::trunc : (std::ios::openmode)0));
    }

    std::fstream file;
#endif // BOOST_ASIO_HAS_IO_URING
};

Util::File::~File() = default;

Util::File::File() = default;

Util::File::File([[maybe_unused]] IOContext &ioc, std::filesystem::path path, bool writable, bool readable) :
#ifdef BOOST_ASIO_HAS_IO_URING
    file(std::make_unique<Stream>(ioc, path, getFlags(writable, readable))),
#else // BOOST_ASIO_HAS_IO_URING
    file(std::make_unique<Stream>(path, readable, writable)),
#endif // BOOST_ASIO_HAS_IO_URING
    path(std::move(path))
{
#ifndef BOOST_ASIO_HAS_IO_URING
    seek(0);
#endif // BOOST_ASIO_HAS_IO_URING
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
#ifdef BOOST_ASIO_HAS_IO_URING
        auto [e, n] = co_await file->file.async_read_some(boost::asio::buffer(buffer),
                                                          boost::asio::as_tuple(boost::asio::use_awaitable));
#else // BOOST_ASIO_HAS_IO_URING
        // Even if we got end-of-file before, we might have more to read now.
        file->file.clear(std::ios::goodbit);

        // Try to read from the file.
        try {
            file->file.read((char *)buffer.data(), buffer.size());
        }
        catch (const std::ios::failure &) {
            if (!file->file.eof()) {
                throw; // Only end-of-file is acceptable as a failure mode.
            }
        }

        // Figure out how much we read and if we got to the end of the file.
        size_t n = file->file.gcount();
        if (n == 0 && file->file.eof()) {
            co_return std::vector<std::byte>();
        }
        assert(n > 0);
#endif // BOOST_ASIO_HAS_IO_URING

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

#ifdef BOOST_ASIO_HAS_IO_URING
        // Handle end of file by returning empty.
        if (e == boost::asio::error::eof) {
            co_return std::vector<std::byte>();
        }

        // Other errors.
        else if (e) {
            throw std::runtime_error("Error reading file " + (std::string)path + ": " + e.message());
        }
#endif // BOOST_ASIO_HAS_IO_URING
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
#ifdef BOOST_ASIO_HAS_IO_URING
    co_await boost::asio::async_read(file->file, boost::asio::mutable_buffer(result.data(), result.size()),
                                     boost::asio::use_awaitable);
#else // BOOST_ASIO_HAS_IO_URING
    file->file.read((char *)result.data(), result.size());
#endif // BOOST_ASIO_HAS_IO_URING
    co_return result;
}

Awaitable<void> Util::File::write(std::span<const std::byte> data)
{
#ifdef BOOST_ASIO_HAS_IO_URING
    co_await boost::asio::async_write(file->file, boost::asio::const_buffer(data.data(), data.size()),
                                      boost::asio::use_awaitable);
#else // BOOST_ASIO_HAS_IO_URING
    /* This seek, write, seek operation makes it so that we always use the read position indicator for writing too, and
       keeps the read position indicator in sync with where we wrote. */
    file->file.seekp(file->file.tellg());
    file->file.write((const char *)data.data(), data.size());
    file->file.seekg(file->file.tellp());
    co_return;
#endif // BOOST_ASIO_HAS_IO_URING
}

void Util::File::seek(size_t offset)
{
#ifdef BOOST_ASIO_HAS_IO_URING
    [[maybe_unused]] uint64_t newOffset = file->file.seek(offset, boost::asio::stream_file::seek_basis::seek_set);
    assert(offset == newOffset);
#else // BOOST_ASIO_HAS_IO_URING
    file->file.seekg(offset);
#endif // BOOST_ASIO_HAS_IO_URING
}

void Util::File::seekToEnd()
{
#ifdef BOOST_ASIO_HAS_IO_URING
    file->file.seek(0, boost::asio::stream_file::seek_basis::seek_end);
#else // BOOST_ASIO_HAS_IO_URING
    file->file.seekg(0, std::ios::end);
#endif // BOOST_ASIO_HAS_IO_URING
}
