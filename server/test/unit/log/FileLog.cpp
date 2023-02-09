#include "log/FileLog.hpp"

#define LogType FileLog
#include "LogTests.hpp"

#include "util/asio.hpp"
#include "util/util.hpp"

#include "coro_test.hpp"

#include <regex>


namespace
{

std::unique_ptr<Log::Log> createLog(IOContext &ioc, ::Log::Level minLevel)
{
    return std::make_unique<Log::FileLog>(ioc, "/tmp/LVSS_FileLog_Test.log", minLevel, false);
}

std::string regexReplace(std::string_view string, std::string_view pattern, std::string_view replacement)
{
    return std::regex_replace(std::string(string), std::regex(std::string(pattern)), std::string(replacement));
}

void checkLogContents(std::string_view ref)
{
    std::vector<std::byte> data = Util::readFile("/tmp/LVSS_FileLog_Test.log");
    std::string_view test((const char *)data.data(), data.size());

    std::string replacedTest = regexReplace(regexReplace(test, "[0-9]{16,}", "TIMESTAMP"), "[0-9]+", "DURATION");
    EXPECT_EQ(std::string(ref), replacedTest) << test;
}

TEST(FileLog, Format)
{
    IOContext ioc;
    std::unique_ptr<Log::Log> logPtr = createLog(ioc);
    Log::Log &log = *logPtr;
    Log::Context context = log("context");

    testCoSpawn([&context]() -> Awaitable<void> {
        context << Log::Level::info << "Message";
        co_return;
    }, ioc);
    ioc.run();

    checkLogContents("{\"contextIndex\":DURATION,\"contextName\":\"\",\"contextTime\":DURATION,\"kind\":\"log\","
                     "\"level\":\"info\",\"logTime\":DURATION,\"message\":\"created\",\"systemTime\":TIMESTAMP}\n"
                     "{\"contextIndex\":DURATION,\"contextName\":\"context\",\"contextTime\":DURATION,"
                     "\"kind\":\"log context\",\"level\":\"info\",\"logTime\":DURATION,\"message\":\"created\","
                     "\"systemTime\":TIMESTAMP}\n"
                     "{\"contextIndex\":DURATION,\"contextName\":\"context\",\"contextTime\":DURATION,"
                     "\"level\":\"info\",\"logTime\":DURATION,\"message\":\"Message\",\"systemTime\":TIMESTAMP}\n");
}

} // namespace
