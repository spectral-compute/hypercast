#include "asio.hpp"

#include "log/Log.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

void spawnDetached(IOContext &ioc, std::move_only_function<Awaitable<void>()> fn)
{
    boost::asio::co_spawn((boost::asio::io_context &)ioc, [fn = std::move(fn)]() mutable -> boost::asio::awaitable<void> {
        return fn();
    }, boost::asio::detached);
}

void spawnDetached(IOContext &ioc, Log::Context &log, std::move_only_function<Awaitable<void>()> fn, Log::Level level)
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
