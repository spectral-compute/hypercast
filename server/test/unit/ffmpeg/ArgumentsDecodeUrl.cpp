#include "ffmpeg/Arguments.hpp"

#include <gtest/gtest.h>

namespace
{

TEST(FfmpegArguments, DecodeUrlNormal)
{
    EXPECT_EQ(Ffmpeg::Arguments::decodeUrl("http://example.com/spiders", "cute"), "http://example.com/spiders");
}

TEST(FfmpegArguments, DecodeUrlIngest)
{
    EXPECT_EQ(Ffmpeg::Arguments::decodeUrl("ingest_http://example.com/spiders", "cute"),
              "http://example.com/spiders/cute");
}

} // namespace
