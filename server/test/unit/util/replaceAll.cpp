#include "util/util.hpp"

#include <gtest/gtest.h>

TEST(replaceAll, Simple)
{
    EXPECT_EQ("Kittens are cute :-D, as are cats :-D: enjoy!",
              Util::replaceAll("Kittens are cute :), as are cats :): enjoy!", ":)", ":-D"));
}

TEST(replaceAll, None)
{
    EXPECT_EQ("Kittens are cute.", Util::replaceAll("Kittens are cute.", ":)", ":-D"));
}

TEST(replaceAll, Entire)
{
    EXPECT_EQ("Cats", Util::replaceAll("Kittens", "Kittens", "Cats"));
}

