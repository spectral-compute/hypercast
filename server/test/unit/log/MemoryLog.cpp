#include "log/MemoryLog.hpp"

#define LogType MemoryLog
#include "LogTests.hpp"

namespace
{

std::unique_ptr<Log::Log> createLog(IOContext &ioc, Log::Level minLevel)
{
    return std::make_unique<Log::MemoryLog>(ioc, minLevel, false);
}

} // namespace
