#include "asio.hpp"

#include "log/Log.hpp"

#include <boost/asio/co_spawn.hpp>

void spawnDetached(IOContext &ioc, Log::Context &log, std::function<Awaitable<void>()> fn, Log::Level level)
{
    spawnDetached(ioc, [fn = std::move(fn), &log, level]() mutable -> Awaitable<void> {
        try {
            co_await fn();
        }
        catch (const std::exception &e) {
            log << "exception" << level << e.what();
        }
        catch (...) {
            log << "exception" << level << "Unknown.";
        }
    });
}

Awaitable<void> awaitTree(std::span<Awaitable<void>> awaitables)
{
    if (awaitables.empty()) {}
    else if (awaitables.size() == 1) {
        co_await std::move(awaitables[0]);
    }
    else {
        co_await (awaitTree(awaitables.subspan(0, awaitables.size() / 2)) &&
                  awaitTree(awaitables.subspan(awaitables.size() / 2)));
    }
}
