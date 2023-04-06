#include "util/RaiiFn.hpp"

#include <gtest/gtest.h>

TEST(RaiiFn, Simple)
{
    bool marker = false;
    {
        Util::RaiiFn fn([&]() { marker = true; });
        EXPECT_FALSE(marker);
    }
    EXPECT_TRUE(marker);
}
