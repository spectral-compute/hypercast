#include "Log.hpp"

#include <deque>

namespace Log
{

class MemoryLog final : public Log
{
public:
    ~MemoryLog() override;
    explicit MemoryLog(IOContext &ioc, Level minLevel, bool print);

private:
    Awaitable<Item> load(size_t index) const override;
    Awaitable<void> store(Item item) override;

    /**
     * The in-memory storage of the log.
     *
     * This is a std::deque rather than a std::vector so that every store is constant-time (rather than just amortized,
     * which would cause the program to occasionally stall).
     */
    std::deque<Item> items;
};

} // namespace Log
