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
