#pragma once

#include "util/asio.hpp"

namespace Config
{

class Root;

/**
 * Fill in defaults for a configuration.
 */
Awaitable<void> fillInDefaults(IOContext &ioc, Root &config);

} // namespace Config
