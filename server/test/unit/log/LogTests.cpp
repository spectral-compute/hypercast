/**
 * @file Tests for a subclass of Log::Log.
 *
 * Since we have more than one subclass of Log::Log, it's useful to apply the same tests to all of them.
 */

#include "log/Log.hpp"

#include "coro_test.hpp"

#include <vector>

void checkLogItem(const Log::Item &ref, const Log::Item &test, bool checkTimestmaps = true);

void checkLogItems(const std::vector<::Log::Item> &ref, const std::vector<::Log::Item> &test)
{
    EXPECT_EQ(ref.size(), test.size());
    for (size_t i = 0; i < std::min(ref.size(), test.size()); i++) {
        checkLogItem(ref[i], test[i], false);
    }
}

Awaitable<std::vector<Log::Item>> extractLog(const Log::Log &log)
{
    size_t size = log.size();
    std::vector<Log::Item> result;
    result.reserve(size);

    for (size_t i = 0; i < size; i++) {
        result.emplace_back(co_await log[i]);
    }

    co_return result;
}

namespace
{

Awaitable<void> check(const std::vector<::Log::Item> &ref, Log::Log &log)
{
    checkLogItems(ref, co_await extractLog(log));
}

} // namespace

#define LOG_TEST(TestName, LogName, IOContextName) \
    void LOG_TEST__##TestName(Log::Log &LogName, IOContext &IOContextName)

LOG_TEST(Simple, log, ioc)
{
    testCoSpawn([&log]() -> Awaitable<void> {
        {
            Log::Context context = log("context");
            context << Log::Level::info << "Message";
        }
        co_await check({
            {
                .kind = "log",
                .message = "created"
            },
            {
                .level = Log::Level::debug,
                .kind = "log context",
                .message = "created",
                .contextName = "context"
            },
            {
                .message = "Message",
                .contextName = "context"
            },
            {
                .level = Log::Level::debug,
                .kind = "log context",
                .message = "destroyed",
                .contextName = "context"
            }
        }, log);
    }, ioc);
    ioc.run();
}

LOG_TEST(Separate, log, ioc)
{
    testCoSpawn([&log]() -> Awaitable<void> {
        Log::Context context = log("context");
        context << Log::Level::info << "Message";
        co_return;
    }, ioc);
    ioc.run();
    ioc.reset();

    // This makes sure we use the load method.
    testCoSpawn([&log]() -> Awaitable<void> {
        co_await check({
            {
                .kind = "log",
                .message = "created"
            },
            {
                .level = Log::Level::debug,
                .kind = "log context",
                .message = "created",
                .contextName = "context"
            },
            {
                .message = "Message",
                .contextName = "context"
            },
            {
                .level = Log::Level::debug,
                .kind = "log context",
                .message = "destroyed",
                .contextName = "context"
            }
        }, log);
    }, ioc);
    ioc.run();
}

LOG_TEST(Wait, log, ioc)
{
    Log::Context context = log("context");

    testCoSpawn([&log]() -> Awaitable<void> {
        co_await log.wait();
        co_await check({
            {
                .kind = "log",
                .message = "created"
            },
            {
                .level = Log::Level::debug,
                .kind = "log context",
                .message = "created",
                .contextName = "context"
            },
            {
                .message = "Message",
                .contextName = "context"
            }
        }, log);
    }, ioc);

    testCoSpawn([&context]() -> Awaitable<void> {
        context << Log::Level::info << "Message";
        co_return;
    }, ioc);

    ioc.run();
}

LOG_TEST(NoWait, log, ioc)
{
    Log::Context context = log("context");

    // this works because the coroutines execute in order.
    testCoSpawn([&log]() -> Awaitable<void> {
        co_await check({
            {
                .kind = "log",
                .message = "created"
            },
            {
                .level = Log::Level::debug,
                .kind = "log context",
                .message = "created",
                .contextName = "context"
            }
        }, log);
    }, ioc);

    testCoSpawn([&context]() -> Awaitable<void> {
        context << Log::Level::info << "Message";
        co_return;
    }, ioc);

    ioc.run();
}

LOG_TEST(Long, log, ioc)
{
    constexpr int count = 5000;

    /* Build the reference. */
    // Log and context creation.
    std::vector<Log::Item> ref = {
        {
            .kind = "log",
            .message = "created"
        },
        {
            .level = Log::Level::debug,
            .kind = "log context",
            .message = "created",
            .contextName = "context"
        }
    };

    // The items we're adding.
    for (int i = 0; i < count; i++) {
        ref.emplace_back(Log::Item{
            .message = "Message: " + std::to_string(i),
            .contextName = "context"
        });
    }

    // Context destruction.
    ref.emplace_back(Log::Item{
        .level = Log::Level::debug,
        .kind = "log context",
        .message = "destroyed",
        .contextName = "context"
    });

    /* Run the test. */
    // Add everything to the queue.
    testCoSpawn([&log]() -> Awaitable<void> {
        Log::Context context = log("context");
        for (int i = 0; i < count; i++) {
            context << Log::Level::info << "Message: " << std::to_string(i);
        }
        co_return;
    }, ioc);
    ioc.run();
    ioc.reset();

    // Check the contents of the log. This is separate so that store has to have been called.
    testCoSpawn([&log, &ref]() -> Awaitable<void> {
        co_await check(ref, log);
    }, ioc);
    ioc.run();
}
