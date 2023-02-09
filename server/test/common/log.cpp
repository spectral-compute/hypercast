#include "log.hpp"

#include "util/asio.hpp"

#include <stdexcept>

ExpectNeverLog::~ExpectNeverLog() = default;

ExpectNeverLog::ExpectNeverLog(IOContext &ioc, ::Log::Level minLevel) : Log(minLevel, true, ioc) {}

Awaitable<Log::Item> ExpectNeverLog::load(size_t) const
{
    throw std::runtime_error("Cannot load from ExpectNeverLog.");
}

Awaitable<void> ExpectNeverLog::store(::Log::Item)
{
    throw std::runtime_error("Cannot store to ExpectNeverLog.");
}
