#include "ArgumentsTestImpl.hpp"

#include "ffmpeg/Arguments.hpp"

#include <gtest/gtest.h>

#include <span>

namespace
{

std::string toPrintable(std::span<const std::string> strings)
{
    std::string result;
    for (const std::string &string: strings) {
        result += "    " + string + "\n";
    }
    result.resize(result.size() - 1);
    return result;
}

} // namespace

void check(const std::vector<std::string> &ref, const Ffmpeg::Arguments &test)
{
    EXPECT_EQ(ref, test.getFfmpegArguments());
    if (ref != test.getFfmpegArguments()) {
        EXPECT_EQ(toPrintable(ref) , toPrintable(test.getFfmpegArguments()));
    }
}
