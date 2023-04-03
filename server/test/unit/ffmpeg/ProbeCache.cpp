#include "ffmpeg/ProbeCache.hpp"

#include <gtest/gtest.h>

namespace
{

TEST(ProbeCache, Simple)
{
    Ffmpeg::ProbeCache cache;
    cache.insert({}, "test", {});

    EXPECT_TRUE(cache.contains("test"));

    const MediaInfo::SourceInfo *sourceInfo = cache["test", {}];
    ASSERT_NE(nullptr, sourceInfo);
    EXPECT_EQ(MediaInfo::SourceInfo{}, *sourceInfo);
}

TEST(ProbeCache, NotFound)
{
    Ffmpeg::ProbeCache cache;

    EXPECT_FALSE(cache.contains("test"));

    const MediaInfo::SourceInfo *sourceInfo = cache["test", {}];
    EXPECT_EQ(nullptr, sourceInfo);
}

TEST(ProbeCache, Arguments)
{
    Ffmpeg::ProbeCache cache;
    cache.insert({ .video = MediaInfo::VideoStreamInfo{ .width = 1 } }, "test", { "a" });
    cache.insert({ .video = MediaInfo::VideoStreamInfo{ .width = 2 } }, "test", { "b" });

    EXPECT_TRUE(cache.contains("test"));

    {
        const MediaInfo::SourceInfo *sourceInfo = cache["test", {}];
        EXPECT_EQ(nullptr, sourceInfo);
    }
    {
        const MediaInfo::SourceInfo *sourceInfo = cache["test", { "a" }];
        ASSERT_NE(nullptr, sourceInfo);
        EXPECT_EQ(MediaInfo::SourceInfo{ .video = MediaInfo::VideoStreamInfo{ .width = 1 } }, *sourceInfo);
    }
    {
        const MediaInfo::SourceInfo *sourceInfo = cache["test", { "b" }];
        ASSERT_NE(nullptr, sourceInfo);
        EXPECT_EQ(MediaInfo::SourceInfo{ .video = MediaInfo::VideoStreamInfo{ .width = 2 } }, *sourceInfo);
    }
    {
        const MediaInfo::SourceInfo *sourceInfo = cache["test", { "c" }];
        EXPECT_EQ(nullptr, sourceInfo);
    }
    {
        const MediaInfo::SourceInfo *sourceInfo = cache["fluff", { "a" }];
        EXPECT_EQ(nullptr, sourceInfo);
    }
}

} // namespace
