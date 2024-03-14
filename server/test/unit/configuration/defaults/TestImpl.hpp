#pragma once

#include "util/awaitable.hpp"

class IOContext;

namespace Config
{

class Root;

} // namespace Config

/**
 * Like Config::fillInDefaults, but provides the probe function.
 */
Awaitable<void> fillInDefaults(IOContext &ioc, Config::Root &config);
