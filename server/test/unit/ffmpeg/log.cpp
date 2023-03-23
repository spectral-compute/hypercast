#include "ffmpeg/log.hpp"

#include <gtest/gtest.h>

TEST(FfmpegLog, AllFields)
{
    Ffmpeg::ParsedFfmpegLogLine parsedLine("[component @ address] [info] message");

    EXPECT_EQ(Log::Level::info, parsedLine.level);
    EXPECT_EQ("info", parsedLine.levelString);
    EXPECT_EQ("component", parsedLine.source);
    EXPECT_EQ("address", parsedLine.sourceAddress);
    EXPECT_EQ("message", parsedLine.message);

    EXPECT_EQ((nlohmann::json{
        { "message", "message" },
        { "level", "info" },
        { "source", "component" },
        { "source_address", "address" }
    }), (nlohmann::json)parsedLine);
}

TEST(FfmpegLog, NoSource)
{
    Ffmpeg::ParsedFfmpegLogLine parsedLine("[verbose] A more complex message.");

    EXPECT_EQ(Log::Level::debug, parsedLine.level);
    EXPECT_EQ("verbose", parsedLine.levelString);
    EXPECT_EQ("", parsedLine.source);
    EXPECT_EQ("", parsedLine.sourceAddress);
    EXPECT_EQ("A more complex message.", parsedLine.message);

    EXPECT_EQ((nlohmann::json{
        { "message", "A more complex message." },
        { "level", "verbose" }
    }), (nlohmann::json)parsedLine);
}

TEST(FfmpegLog, Malformed)
{
    Ffmpeg::ParsedFfmpegLogLine parsedLine("A malformed message.");

    EXPECT_EQ(Log::Level::error, parsedLine.level);
    EXPECT_EQ("", parsedLine.levelString);
    EXPECT_EQ("", parsedLine.source);
    EXPECT_EQ("", parsedLine.sourceAddress);
    EXPECT_EQ("A malformed message.", parsedLine.message);

    EXPECT_EQ((nlohmann::json{ { "message", "A malformed message." } }), (nlohmann::json)parsedLine);
}
