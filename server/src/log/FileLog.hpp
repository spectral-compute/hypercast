#include "Log.hpp"

#include "util/File.hpp"
#include "util/Mutex.hpp"

#include <filesystem>
#include <deque>
#include <string>
#include <vector>

namespace Log
{

class FileLog final : public Log
{
public:
    ~FileLog() override;
    explicit FileLog(IOContext &ioc, const std::filesystem::path &path, Level minLevel, bool print,
                     size_t endCacheSize = 1024, size_t loadCacheSize = 256);

private:
    Awaitable<Item> load(size_t index) const override;
    Awaitable<void> store(Item item) override;

    /**
     * A mutex to protect the file handle and the rest of the structure so we don't interpose IO and seek operations.
     */
    mutable Mutex mutex;

    /**
     * The file the log is written to (and read from).
     */
    mutable Util::File file;

    /**
     * File offsets for the start of every loadCacheSize log items.
     *
     * This includes 0 to simplify the logic. The last entry is the offset, in bytes, into the file of the log entry to
     * be written next (which may not be at an index that is a multiple of loadCacheSize).
     *
     * The reason for storing offsets at all is so load can find the right place in the file to load from. The reason
     * for only storing them at intervals is to save memory.
     *
     * The reason this operates in terms of loadCacheSize rather than some other value is that it's easier to make the
     * load cache size be the same as the number of items between offsets we keep.
     *
     * The reason this is a std::deque is to guarantee constant time insertion rather than just ammortized constant time
     * insertion.
     */
    std::deque<size_t> offsets;

    /**
     * A cache of the last N items.
     *
     * This is useful because the most common read operation is likely to be to read from the end of the log.
     */
    std::deque<Item> endCache;

    /**
     * The maximum size of the cache.
     */
    const size_t endCacheSize;

    /**
     * The index of the first item in endCache.
     *
     * Log only makes the assumption that store has completed once store actually returns. But load could be called for
     * an earlier item between when store returns (thus unlocking the mutex) and when its caller resumes (due to the
     * fact that store is a coroutine), which would make it unknowable whether getWrittenItemCount reflected the old or
     * new value.
     */
    size_t endCacheStart = 0;

    /**
     * The load method loads loadCacheSize-many items at once (except at the end of the log file). This is where they
     * get put.
     *
     * The strings are cached rather than the items themselves so that we don't have to JSON decode everything in the
     * cache just for one item.
     */
    mutable std::vector<std::string> loadCache;

    /**
     * Number of log items after each entry in offsets before the next entry in offsets.
     */
    const size_t loadCacheSize;

    /**
     * The index of the first log item cached in loadCache.
     */
    mutable size_t loadCacheStart = 0;
};

} // namespace Log
