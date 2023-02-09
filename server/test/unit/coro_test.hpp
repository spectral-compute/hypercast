#pragma once

#include "util/asio.hpp"

#include <gtest/gtest.h>

#include <functional>

/**
 * Run a function in a coroutine, and print any exceptions that happen.
 */
void testCoSpawn(std::function<Awaitable<void>()> fn, IOContext &ioc);

/**
 * Create a GTest test that uses coroutines.
 */
#define CORO_TEST(TestSuiteName, TestName, IOContextName) \
    static Awaitable<void> CORO_TEST__##TestSuiteName__##TestName(IOContext &IOContextName); \
    TEST(TestSuiteName, TestName) \
    { \
        IOContext ioc; \
        testCoSpawn([&ioc]() { return CORO_TEST__##TestSuiteName__##TestName(ioc); }, ioc); \
        ioc.run(); \
    } \
    static Awaitable<void> CORO_TEST__##TestSuiteName__##TestName(IOContext &IOContextName)
