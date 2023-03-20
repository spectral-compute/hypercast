#pragma once

/**
 * @file Tests for a subclass of Log::Log.
 *
 * Since we have more than one subclass of Log::Log, it's useful to apply the same tests to all of them.
 */

#include "util/asio.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace Log
{

class Log;

} // namespace Log

namespace
{

std::unique_ptr<Log::Log> createLog(IOContext &ioc, Log::Level minLevel = Log::Level::info); // NOLINT

} // namespace

/**
 * A macro to create a GTest test, create a log of the type under test, and call the implementation of the test.
 */
#define LOG_TEST(TestName, ...) \
    void LOG_TEST__##TestName(Log::Log &log, IOContext &ioc); \
    namespace \
    { \
        TEST(LogType, TestName) \
        { \
            IOContext ioc; \
            std::unique_ptr<Log::Log> log = createLog(ioc __VA_OPT__(,) __VA_ARGS__); \
            LOG_TEST__##TestName(*log, ioc); \
        } \
    }

/* These are a bit like prototypes of the tests in LogTests.cpp. */
LOG_TEST(Simple)
LOG_TEST(Separate)
LOG_TEST(Wait)
LOG_TEST(NoWait)
LOG_TEST(Long)
