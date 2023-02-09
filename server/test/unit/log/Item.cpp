#include "log/Item.hpp"

#include <gtest/gtest.h>

#include <clocale>

namespace
{

using LogDuration = std::chrono::microseconds;

} // namespace

bool Log::Item::operator==(const Log::Item &) const = default;

void checkLogItem(const Log::Item &ref, const Log::Item &test, bool checkTimestmaps = true)
{
    Log::Item modRef = ref;

    /* Correct for clock rounding. */
    modRef.logTime = std::chrono::duration_cast<std::chrono::steady_clock::duration>
                        (std::chrono::duration_cast<LogDuration>(ref.logTime));
    modRef.contextTime = std::chrono::duration_cast<std::chrono::steady_clock::duration>
                         (std::chrono::duration_cast<LogDuration>(ref.contextTime));
    modRef.systemTime =
        std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>
        (std::chrono::duration_cast<LogDuration>(ref.systemTime.time_since_epoch())));

    /* Compare the object. */
    // Compare each field first so we see what changed.
    if (checkTimestmaps) {
        EXPECT_EQ(modRef.logTime, test.logTime);
        EXPECT_EQ(modRef.contextTime, test.contextTime);
        EXPECT_EQ(modRef.systemTime, test.systemTime);
    }
    EXPECT_EQ(modRef.level, test.level);
    EXPECT_EQ(modRef.kind, test.kind);
    EXPECT_EQ(modRef.message, test.message);
    EXPECT_EQ(modRef.contextName, test.contextName);
    EXPECT_EQ(modRef.contextIndex, test.contextIndex);

    // Compare the whole object, so we don't get caught out by a new field.
    if (!checkTimestmaps) {
        modRef.logTime = test.logTime;
        modRef.contextTime = test.contextTime;
        modRef.systemTime = test.systemTime;
    }
    EXPECT_EQ(modRef, test);
}

namespace
{

/**
 * A RAII object to set the locale to a standard value for testing, and restore it afterwards.
 */
class LocaleRaii final
{
public:
    ~LocaleRaii()
    {
        for (size_t i = 0; i < categories.size(); i++) {
            int category = categories[i];
            const std::string &locale = locales[i];

            if (!std::setlocale(category, locale.c_str())) {
                fprintf(stderr, "Failed to restore locale.");
            }
        }
    }

    LocaleRaii(const char *newLocale = "en_GB.UTF-8")
    {
        for (size_t i = 0; i < categories.size(); i++) {
            int category = categories[i];
            std::string &locale = locales[i];

            const char *oldLocale = std::setlocale(category, nullptr);
            if (!oldLocale) {
                fprintf(stderr, "Failed to save locale.");
            }
            locale = oldLocale;
            if (!std::setlocale(category, newLocale)) {
                fprintf(stderr, "Failed to set locale.");
            }
        }
    }

private:
    static constexpr std::array<int, 6> categories = {
        LC_ALL,
        LC_COLLATE,
        LC_CTYPE,
        LC_MONETARY,
        LC_NUMERIC,
        LC_TIME
    };
    std::array<std::string, categories.size()> locales;
};

} // namespace

TEST(LogItem, Default)
{
    Log::Item item;

    /* Check the JSON serialization/deserialization. */
    std::string jsonString = item.toJsonString();
    EXPECT_EQ("{\"contextIndex\":0,\"contextName\":\"\",\"contextTime\":0,\"level\":\"info\",\"logTime\":0,"
              "\"message\":\"\",\"systemTime\":0}", jsonString);
    checkLogItem(item, Log::Item::fromJsonString(jsonString));

    /* Check the string formatting. */
    LocaleRaii locale;
    EXPECT_EQ("[Info] @ 0.000000 s = [0] + 0.000000 s = Thu Jan  1 01:00:00 1970: [] ", item.format());
}

TEST(LogItem, Simple)
{
    Log::Item item = {
        .logTime = std::chrono::seconds(314159),
        .contextTime = std::chrono::seconds(271828),
        .systemTime = std::chrono::system_clock::time_point(std::chrono::seconds(1 << 30)),
        .level = Log::Level::warning,
        .kind = "Simple",
        .message = "Enjoy!",
        .contextName = "Simple log item",
        .contextIndex = 42
    };

    /* Check the JSON serialization/deserialization. */
    std::string jsonString = item.toJsonString();
    EXPECT_EQ("{\"contextIndex\":42,\"contextName\":\"Simple log item\",\"contextTime\":271828000000,"
              "\"kind\":\"Simple\",\"level\":\"warning\",\"logTime\":314159000000,\"message\":\"Enjoy!\","
              "\"systemTime\":1073741824000000}", jsonString);
    checkLogItem(item, Log::Item::fromJsonString(jsonString));

    /* Check the string formatting. */
    LocaleRaii locale;
    EXPECT_EQ("[Warning] @ 314159.000000 s = Simple log item[42] + 271828.000000 s = Sat Jan 10 13:37:04 2004: "
              "[Simple] Enjoy!", item.format());

    std::string colour = item.format(true);
    EXPECT_EQ("[\x1b[33;1mWarning\x1b[m] @ \x1b[34m314159.000000 s\x1b[m = "
              "\x1b[36;1mSimple log item\x1b[m[\x1b[36;1m42\x1b[m] + \x1b[34m271828.000000 s\x1b[m = "
              "\x1b[34mSat Jan 10 13:37:04 2004\x1b[m: [\x1b[35;1mSimple\x1b[m] Enjoy!", colour);
}

TEST(LogItem, Now)
{
    Log::Item item = {
        .logTime = std::chrono::steady_clock::now().time_since_epoch(),
        .contextTime = std::chrono::steady_clock::now().time_since_epoch(),
        .systemTime = std::chrono::system_clock::now(),
        .level = Log::Level::error,
        .kind = "Now",
        .message = "Enjoy!",
        .contextName = "Now log item",
        .contextIndex = 314159
    };
    checkLogItem(item, Log::Item::fromJsonString(item.toJsonString()));
}
