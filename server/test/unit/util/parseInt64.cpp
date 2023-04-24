#include "util/util.hpp"

#include <gtest/gtest.h>

TEST(parseInt64, Simple)
{
    EXPECT_EQ(0, Util::parseInt64("0"));
    EXPECT_EQ(42, Util::parseInt64("42"));
    EXPECT_EQ(-42, Util::parseInt64("-42"));
    EXPECT_EQ(1234567890123456789ll, Util::parseInt64("1234567890123456789"));
}

TEST(parseInt64, Bad)
{
    EXPECT_THROW(Util::parseInt64(""), std::invalid_argument);
    EXPECT_THROW(Util::parseInt64("x"), std::invalid_argument);
    EXPECT_THROW(Util::parseInt64("42.0"), std::invalid_argument);
    EXPECT_THROW(Util::parseInt64("42x"), std::invalid_argument);
    EXPECT_THROW(Util::parseInt64("x42"), std::invalid_argument);
    EXPECT_THROW(Util::parseInt64("999999999999999999999999999"), std::range_error);
}
