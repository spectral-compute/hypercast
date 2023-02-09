#pragma once

#include "log/Log.hpp"

/**
 * A log that's never expected to contain anything.
 */
class ExpectNeverLog final : public Log::Log
{
public:
    ~ExpectNeverLog() override;

    /**
     * Constructor :)
     *
     * @param minLevel The minimum level that's expected to never happen.
     */
    ExpectNeverLog(IOContext &ioc, ::Log::Level minLevel = ::Log::Level::warning);

private:
    Awaitable<::Log::Item> load(size_t index) const override;
    Awaitable<void> store(::Log::Item item) override;
};
