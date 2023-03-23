#include "util/subprocess.hpp"

#include "coro_test.hpp"

#include <stdexcept>

// A lot of the functionality of Subprocess::Subprocess is tested by Subprocess::getStdout's tests.

CORO_TEST(Subprocess, ReadLine, ioc)
{
    Subprocess::Subprocess subprocess(ioc, "bash", {"-c", "echo triangle ; echo hexagon"}, false, true, false);
    EXPECT_EQ("triangle", co_await subprocess.readStdoutLine());
    EXPECT_EQ("hexagon", co_await subprocess.readStdoutLine());
    EXPECT_EQ(std::nullopt, co_await subprocess.readStdoutLine());
}

CORO_TEST(Subprocess, ReadLineCr, ioc)
{
    Subprocess::Subprocess subprocess(ioc, "bash", {"-c", "echo -e \"triangle\\rhexagon\""}, false, true, false);
    EXPECT_EQ("triangle", co_await subprocess.readStdoutLine());
    EXPECT_EQ("hexagon", co_await subprocess.readStdoutLine());
    EXPECT_EQ(std::nullopt, co_await subprocess.readStdoutLine());
}

CORO_TEST(Subprocess, ReadLineCrLf, ioc)
{
    Subprocess::Subprocess subprocess(ioc, "bash", {"-c", "echo -e \"triangle\\r\\nhexagon\""}, false, true, false);
    EXPECT_EQ("triangle", co_await subprocess.readStdoutLine());
    EXPECT_EQ("hexagon", co_await subprocess.readStdoutLine());
    EXPECT_EQ(std::nullopt, co_await subprocess.readStdoutLine());
}
