/**
 * @file Tests (including those from LogTests.hpp) that inspect the correct operation of the Log::Log API.
 */

#include "log/Log.hpp"

#include "coro_test.hpp"

#include <vector>

void checkLogItems(const std::vector<::Log::Item> &ref, const std::vector<::Log::Item> &test);

namespace
{

/**
 * An implementation of Log::Log that records the items in memory so we can check them.
 */
class TestLog final : public Log::Log
{
public:
    explicit TestLog(IOContext &ioc, ::Log::Level minLevel = ::Log::Level::info) : Log(minLevel, false, ioc) {}

    /**
     * Check the contents of the log we stored.
     */
    void checkSync(const std::vector<::Log::Item> &ref) const
    {
        checkLogItems(ref, items);
    }

private:
    Awaitable<::Log::Item> load(size_t index) const override
    {
        EXPECT_GE(items.size(), index);
        co_return items[index];
    }

    Awaitable<void> store(::Log::Item item) override
    {
        EXPECT_EQ(items.size(), getWrittenItemCount());
        items.emplace_back(std::move(item));
        co_return;
    }

    std::vector<::Log::Item> items;
};

TEST(Log, Simple)
{
    IOContext ioc;
    TestLog log(ioc);
    Log::Context context = log("context");

    testCoSpawn([&context]() -> Awaitable<void> {
        context << Log::Level::info << "Message";
        co_return;
    }, ioc);
    ioc.run();

    log.checkSync({
        {
            .kind = "log",
            .message = "created"
        },
        {
            .kind = "log context",
            .message = "created",
            .contextName = "context"
        },
        {
            .message = "Message",
            .contextName = "context"
        }
    });
}

TEST(Log, MinLevel)
{
    IOContext ioc;
    TestLog log(ioc, Log::Level::warning);
    Log::Context context = log("context");

    testCoSpawn([&context]() -> Awaitable<void> {
        context << Log::Level::info << "Message";
        co_return;
    }, ioc);
    ioc.run();

    log.checkSync({}); // All the messages are info level, so they should be filtered out.
}

std::unique_ptr<Log::Log> createLog(IOContext &ioc, ::Log::Level minLevel) // NOLINT
{
    return std::make_unique<TestLog>(ioc, minLevel);
}

} // namespace

#define LogType TestLog
#include "LogTests.hpp"
