#include "ffmpeg/log.hpp"

#include <gtest/gtest.h>

TEST(FfmpegLog, AllFields)
{
    Ffmpeg::ParsedFfmpegLogLine parsedLine("[component @ address] [info] message");

    EXPECT_EQ(Log::Level::info, parsedLine.level);
    EXPECT_EQ("[component @ address] message", parsedLine.message);
}

TEST(FfmpegLog, NoSource)
{
    Ffmpeg::ParsedFfmpegLogLine parsedLine("[verbose] A more complex message.");

    EXPECT_EQ(Log::Level::debug, parsedLine.level);
    EXPECT_EQ("A more complex message.", parsedLine.message);
}

TEST(FfmpegLog, Malformed)
{
    Ffmpeg::ParsedFfmpegLogLine parsedLine("A malformed message.");

    EXPECT_EQ(Log::Level::error, parsedLine.level);
    EXPECT_EQ("A malformed message.", parsedLine.message);
}
