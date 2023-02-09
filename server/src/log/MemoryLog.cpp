#include "MemoryLog.hpp"

#include "util/asio.hpp"

#include <cassert>

Log::MemoryLog::~MemoryLog() = default;
Log::MemoryLog::MemoryLog(IOContext &ioc, Level minLevel, bool print) : Log(minLevel, print, ioc) {}

Awaitable<Log::Item> Log::MemoryLog::load(size_t index) const
{
    assert(items.size() == getWrittenItemCount());
    assert(index < items.size());
    co_return items[index];
}

Awaitable<void> Log::MemoryLog::store(Item item)
{
    items.emplace_back(std::move(item));
    co_return;
}
