#include "util/subprocess.hpp"

#include "coro_test.hpp"

#include <stdexcept>

using namespace std::string_literals;

namespace
{

CORO_TEST(SubprocessGetStdout, Echo, ioc)
{
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

} // namespace
