#include "util/subprocess.hpp"

#include "util/Event.hpp"
#include "util/util.hpp"

#include "coro_test.hpp"
#include "data.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/stream_file.hpp>

#include <stdexcept>

using namespace std::string_literals;

namespace
{

CORO_TEST(SubprocessGetStdout, Echo, ioc)
{
    EXPECT_EQ("hexagon\n", (co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"})));
}

CORO_TEST(SubprocessGetStdout, Twice, ioc)
{
    EXPECT_EQ("hexagon\n", (co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"})));
    EXPECT_EQ("hexagon\n", (co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"})));
}

CORO_TEST(SubprocessGetStdout, EchoMultiline, ioc)
{
    EXPECT_EQ("triangle\nhexagon\n",
              (co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo triangle ; echo hexagon"})));
}

CORO_TEST(SubprocessGetStdout, Large, ioc)
{
    EXPECT_EQ(1000000, (co_await Subprocess::getStdout(ioc, "bash", {"-c", "head -c 1000000 /dev/zero"})).size());
}

CORO_TEST(SubprocessGetStdout, Cat, ioc)
{
    EXPECT_EQ("triangle\nhexagon",
              (co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo triangle ; cat"}, "hexagon")));
}

CORO_TEST(SubprocessGetStdout, FalseStderr, ioc)
{
    try {
        co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo doom 1>&2 && false"});
        ADD_FAILURE() << "No exception.";
    }
    catch (const std::runtime_error &e) {
        EXPECT_EQ("Subprocess bash returned 1, and stderr:\ndoom\n"s, e.what());
    }
}

TEST(SubprocessGetStdout, Spawn)
{
    IOContext ioc;

    std::string result;
    testCoSpawn([&]() -> Awaitable<void> {
        result = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"});
    }, ioc);

    ioc.run();
    EXPECT_EQ("hexagon\n", result);
}

TEST(SubprocessGetStdout, SpawnTwice)
{
    IOContext ioc;

    std::string result1;
    testCoSpawn([&]() -> Awaitable<void> {
        result1 = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"});
    }, ioc);

    std::string result2;
    testCoSpawn([&]() -> Awaitable<void> {
        result2 = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"});
    }, ioc);

    ioc.run();
    EXPECT_EQ("hexagon\n", result1);
    EXPECT_EQ("hexagon\n", result2);
}

CORO_TEST(SubprocessGetStdout, SpawnFromCoro, ioc)
{
    /* Make it possible for this coroutine to tell when its detached child coroutine finishes. */
    Event event(ioc);
    int finished = 0;

    /* Spawn a detached coroutine to run a subprocess. */
    std::string result;
    testCoSpawn([&]() -> Awaitable<void> {
        result = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"});
        finished++;
        event.notifyAll();
        co_return;
    }, ioc);

    /* Wait for the spawned coroutine to finish. */
    while (finished < 1) {
        co_await event.wait();
    }

    /* Check the results. */
    EXPECT_EQ("hexagon\n", result);
}

CORO_TEST(SubprocessGetStdout, SpawnTwiceFromCoro, ioc)
{
    /* Make it possible for this coroutine to tell when its detached child coroutines finish. */
    Event event(ioc);
    int finished = 0;

    /* Spawn a detached coroutine to run subprocesses. */
    // One subprocess.
    std::string result1;
    testCoSpawn([&]() -> Awaitable<void> {
        result1 = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"});
        finished++;
        event.notifyAll();
    }, ioc);

    // Another subprocess.
    std::string result2;
    testCoSpawn([&]() -> Awaitable<void> {
        result2 = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo hexagon"});
        finished++;
        event.notifyAll();
    }, ioc);

    /* Wait for the spawned coroutines to finish. */
    while (finished < 2) {
        co_await event.wait();
    }

    /* Check the results. */
    EXPECT_EQ("hexagon\n", result1);
    EXPECT_EQ("hexagon\n", result2);
}

Awaitable<std::string> hexagons(IOContext &ioc)
{
    std::string hexagon = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo -n hexagon"});
    std::string s = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo -n s"});
    co_return hexagon + s;
}

CORO_TEST(SubprocessGetStdout, CoroStack, ioc)
{
    EXPECT_EQ("hexagons", (co_await hexagons(ioc)));
}

CORO_TEST(SubprocessGetStdout, TwiceCoroStack, ioc)
{
    EXPECT_EQ("hexagons", (co_await hexagons(ioc)));
    EXPECT_EQ("hexagons", (co_await hexagons(ioc)));
}

#ifdef BOOST_ASIO_HAS_IO_URING
/**
 * Test that running a subprocess and reading a file in parallel work.
 *
 * This test produced `epoll re-registration: File exists [system:17]` when ran with count = 2.
 */
Awaitable<void> subprocessAndFileCoro(IOContext &ioc, unsigned int count)
{
    std::filesystem::path path = getSmpteDataPath(1920, 1080, 25, 1, 48000);
    std::vector<std::byte> refData = Util::readFile(path);

    /* Run a subprocess and read a file repeatedly. */
    for (unsigned int i = 0; i < count; i++) {
        // Run a subprocess.
        std::string string = co_await Subprocess::getStdout(ioc, "bash", {"-c", "echo -n hexagon"});
        EXPECT_EQ("hexagon", string);

        // Read a file.
        std::vector<std::byte> data;
        boost::asio::stream_file f(ioc, path, boost::asio::stream_file::read_only);
        while (true) {
            std::byte buffer[4096];
            auto [e, n] = co_await f.async_read_some(boost::asio::buffer(buffer),
                                                     boost::asio::as_tuple(boost::asio::use_awaitable));
            data.insert(data.end(), buffer, buffer + n);

            if (e == boost::asio::error::eof) {
                f.close();
                EXPECT_EQ(refData, data);
                break;
            }
            else if (e) {
                throw std::runtime_error("Error reading file " + path.string() + ": " + e.message());
            }
        }
    }
}

CORO_TEST(SubprocessGetStdout, AndFileRead, ioc)
{
    co_await subprocessAndFileCoro(ioc, 1);
}

CORO_TEST(SubprocessGetStdout, TwiceAndFileRead, ioc)
{
    co_await subprocessAndFileCoro(ioc, 2);
}
#endif // BOOST_ASIO_HAS_IO_URING

} // namespace
