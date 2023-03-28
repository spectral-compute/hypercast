#include "FileLog.hpp"

#include "util/asio.hpp"

Log::FileLog::~FileLog() = default;

Log::FileLog::FileLog(IOContext &ioc, const std::filesystem::path &path, Level minLevel, bool print,
                      size_t endCacheSize, size_t loadCacheSize) :
    Log(minLevel, print, ioc), mutex(ioc),
    file(ioc, path, true, true), endCacheSize(endCacheSize), loadCacheSize(loadCacheSize)
{
    assert(loadCacheSize > 0);
    offsets.emplace_back(0);
}

Awaitable<Log::Item> Log::FileLog::load(size_t index) const
{
    /* Make sure the data structures we're using don't get changed while we're using them (or, in the case of
       reading from file, modifying in the form of seeking the file handle). */
    Mutex::LockGuard lock = co_await mutex.lockGuard();

    /* Try and load the item from the log-end items cache. */
    if (index >= endCacheStart) {
        assert(index - endCacheStart < endCache.size());
        co_return endCache[index - endCacheStart];
    }

    /* Try to load out of the last-load cache. */
    if (index >= loadCacheStart && index < loadCacheStart + loadCache.size()) {
        co_return ::Log::Item::fromJsonString(loadCache[index - loadCacheStart]);
    }

    /* We're actually going to have to load something. */
    // Figure out what we're loading from the file.
    size_t indexIntoOffsets = index / loadCacheSize;
    assert(indexIntoOffsets + 1 < offsets.size());

    size_t startFileOffset = offsets[indexIntoOffsets];
    size_t endFileOffset = offsets[indexIntoOffsets + 1];
    assert(endFileOffset > startFileOffset);

    // Load the data from the file.
    file.seek(startFileOffset);
    std::vector<std::byte> data = co_await file.readExact(endFileOffset - startFileOffset);

    /* Remove the current contents of the load cache, so we can replace it. */
    loadCache.clear();
    loadCache.reserve(loadCacheSize); // I think this is a no-op after the first time, but I'm not certain.

    // Also update the cache start.
    loadCacheStart = indexIntoOffsets * loadCacheSize;

    /* Split the loaded data into strings (one string per line), and decode them into the load cache. */
    std::string_view lines((const char *)data.data(), data.size());
    while (!lines.empty()) {
        // If the load cache is already full, the log file must have had an extra newline we didn't expect.
        if (loadCache.size() == loadCacheSize) {
            throw std::runtime_error("Unexpected extra newline found in loaded log file.");
        }

        // Find the end of the next item.
        size_t newline = lines.find('\n');
        if (newline == std::string::npos) {
            throw std::runtime_error("No newline found in loaded log file data.");
        }

        // Add the line to the cache and move onto the next.
        loadCache.emplace_back(lines.substr(0, newline));
        lines = lines.substr(newline + 1);
    }

    // If we didn't load the last block of items but the laod cache isn't full, or if the item we're looking for is
    // still not in the load cache, then we didn't get as many items as expected.
    if ((offsets.size() != indexIntoOffsets + 2 && loadCache.size() != loadCacheSize) ||
        loadCache.size() <= index - loadCacheStart)
    {
        throw std::runtime_error("Fewer log items loaded from log file than expected.");
    }

    /* The load cache should now contain the item we want. */
    assert(index >= loadCacheStart && index < loadCacheStart + loadCache.size());
    co_return ::Log::Item::fromJsonString(loadCache[index - loadCacheStart]);
}

Awaitable<void> Log::FileLog::store(Item item)
{
    /* Encode the item to a string. This potentially moderately expensive thing can happen while IO happens :) (imagine
       this is the only coroutine that's ready to run but something's already got the lock). */
    std::string jsonString = item.toJsonString();
    assert(jsonString.find('\n') == std::string::npos); // The JSON encoding should not contain any newlines.
    jsonString += "\n"; // Newline separate the items in the log.

    /* Most of the stuff after this point is sensitive to co-occuring loads/stores, especially because it mutates
       stuff, and a load operation could begin as a consequence of the co_await. */
    Mutex::LockGuard lock = co_await mutex.lockGuard();

    /* Write the item to the log file. The co_awaits are the point at which load on this item could be called. */
    file.seekToEnd();
    co_await file.write({(const std::byte *)jsonString.data(), jsonString.size()});

    /* Update the end cache. */
    size_t index = endCacheStart + endCache.size();
    if (endCacheSize > 0) {
        if (endCache.size() == endCacheSize) {
            endCache.pop_front();
            endCacheStart++;
        }
        endCache.emplace_back(std::move(item));
    }

    /* Update the offsets. */
    assert(index == getWrittenItemCount()); // Because stores are sequentialized, and this updates after store returns.
    assert(offsets.size() == (index + loadCacheSize - 1) / loadCacheSize + 1);

    if (index % loadCacheSize == 0) {
        offsets.emplace_back(offsets.back());
    }
    offsets.back() += jsonString.size();

    /* Done :) */
    co_return;
}
