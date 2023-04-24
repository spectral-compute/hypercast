#include "ffmpeg/Timestamp.hpp"

#include <gtest/gtest.h>

TEST(FfmpegTimestamp, Simple)
{
    Ffmpeg::Timestamp ts(2, 3, 5);
    EXPECT_TRUE(ts);
    EXPECT_EQ(2, ts.getValue());
    EXPECT_EQ(3, ts.getTimeBase().first);
    EXPECT_EQ(5, ts.getTimeBase().second);
}

TEST(FfmpegTimestamp, Null)
{
    Ffmpeg::Timestamp ts;
    EXPECT_FALSE(ts);
    EXPECT_EQ(0, ts.getValue());
    EXPECT_EQ(0, ts.getTimeBase().first);
    EXPECT_EQ(0, ts.getTimeBase().second);
}

TEST(FfmpegTimestamp, ValueInSeconds)
{
    Ffmpeg::Timestamp ts(3, 5, 16);
    EXPECT_EQ(0.9375, ts.getValueInSeconds());
}

TEST(FfmpegTimestamp, EqNull)
{
    Ffmpeg::Timestamp a;
    Ffmpeg::Timestamp b;
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(FfmpegTimestamp, Eq)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(2, 3, 5);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(FfmpegTimestamp, EqND)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(2, 6, 10);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(FfmpegTimestamp, EqVN)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(3, 2, 5);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(FfmpegTimestamp, EqVD)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(4, 3, 10);
    EXPECT_FALSE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(FfmpegTimestamp, LtVND)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(7, 11, 13);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(FfmpegTimestamp, LtND)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(2, 11, 13);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(FfmpegTimestamp, LtVD)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(7, 3, 13);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(FfmpegTimestamp, LtVN)
{
    Ffmpeg::Timestamp a(2, 3, 5);
    Ffmpeg::Timestamp b(7, 11, 5);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(a > b);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}
