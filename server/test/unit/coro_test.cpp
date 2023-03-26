#include "coro_test.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

void testCoSpawn(std::function<Awaitable<void>()> fn, IOContext &ioc)
{
    boost::asio::co_spawn((boost::asio::io_context &)ioc, [fn = std::move(fn)]() -> boost::asio::awaitable<void> {
        try {
            co_await fn();
        }
        catch (const std::exception &e) {
            ADD_FAILURE() << "Coroutine exited with exception: " << e.what();
        }
        catch (...) {
            ADD_FAILURE() << "Coroutine exited with an exception.";
        }
    }, boost::asio::detached);
}
