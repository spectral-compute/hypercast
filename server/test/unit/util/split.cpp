#include "util/util.hpp"

#include <gtest/gtest.h>

TEST(split, Simple1)
{
    std::string_view complete = "kitten";
    std::string_view a;
    Util::split(complete, { a });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data() + 6, &*a.end());
}

TEST(split, Simple2)
{
    std::string_view complete = "kitten cat";
    std::string_view a;
    std::string_view b;
    Util::split(complete, { a, b });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data() + 6, &*a.end());
    EXPECT_EQ(complete.data() + 7, &*b.begin());
    EXPECT_EQ(complete.data() + 10, &*b.end());
}

TEST(split, Simple3)
{
    std::string_view complete = "kitten cat lion";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data() + 6, &*a.end());
    EXPECT_EQ(complete.data() + 7, &*b.begin());
    EXPECT_EQ(complete.data() + 10, &*b.end());
    EXPECT_EQ(complete.data() + 11, &*c.begin());
    EXPECT_EQ(complete.data() + 15, &*c.end());
}

TEST(split, Simple4)
{
    std::string_view complete = "kitten cat lion tiger";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    std::string_view d;
    Util::split(complete, { a, b, c, d });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data() + 6, &*a.end());
    EXPECT_EQ(complete.data() + 7, &*b.begin());
    EXPECT_EQ(complete.data() + 10, &*b.end());
    EXPECT_EQ(complete.data() + 11, &*c.begin());
    EXPECT_EQ(complete.data() + 15, &*c.end());
    EXPECT_EQ(complete.data() + 16, &*d.begin());
    EXPECT_EQ(complete.data() + 21, &*d.end());
}

TEST(split, Empty1)
{
    std::string_view complete = " cat lion";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data(), &*a.end());
    EXPECT_EQ(complete.data() + 1, &*b.begin());
    EXPECT_EQ(complete.data() + 4, &*b.end());
    EXPECT_EQ(complete.data() + 5, &*c.begin());
    EXPECT_EQ(complete.data() + 9, &*c.end());
}

TEST(split, Empty2)
{
    std::string_view complete = "kitten  lion";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data() + 6, &*a.end());
    EXPECT_EQ(complete.data() + 7, &*b.begin());
    EXPECT_EQ(complete.data() + 7, &*b.end());
    EXPECT_EQ(complete.data() + 8, &*c.begin());
    EXPECT_EQ(complete.data() + 12, &*c.end());
}

TEST(split, Empty3)
{
    std::string_view complete = "kitten cat ";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data() + 6, &*a.end());
    EXPECT_EQ(complete.data() + 7, &*b.begin());
    EXPECT_EQ(complete.data() + 10, &*b.end());
    EXPECT_EQ(complete.data() + 11, &*c.begin());
    EXPECT_EQ(complete.data() + 11, &*c.end());
}

TEST(split, Empty12)
{
    std::string_view complete = "  lion";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data(), &*a.end());
    EXPECT_EQ(complete.data() + 1, &*b.begin());
    EXPECT_EQ(complete.data() + 1, &*b.end());
    EXPECT_EQ(complete.data() + 2, &*c.begin());
    EXPECT_EQ(complete.data() + 6, &*c.end());
}

TEST(split, Empty13)
{
    std::string_view complete = " cat ";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data(), &*a.end());
    EXPECT_EQ(complete.data() + 1, &*b.begin());
    EXPECT_EQ(complete.data() + 4, &*b.end());
    EXPECT_EQ(complete.data() + 5, &*c.begin());
    EXPECT_EQ(complete.data() + 5, &*c.end());
}

TEST(split, Empty23)
{
    std::string_view complete = "kitten  ";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data() + 6, &*a.end());
    EXPECT_EQ(complete.data() + 7, &*b.begin());
    EXPECT_EQ(complete.data() + 7, &*b.end());
    EXPECT_EQ(complete.data() + 8, &*c.begin());
    EXPECT_EQ(complete.data() + 8, &*c.end());
}

TEST(split, Empty123)
{
    std::string_view complete = "  ";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    Util::split(complete, { a, b, c });

    EXPECT_EQ(complete.data(), &*a.begin());
    EXPECT_EQ(complete.data(), &*a.end());
    EXPECT_EQ(complete.data() + 1, &*b.begin());
    EXPECT_EQ(complete.data() + 1, &*b.end());
    EXPECT_EQ(complete.data() + 2, &*c.begin());
    EXPECT_EQ(complete.data() + 2, &*c.end());
}

TEST(split, TooFewSeparators1)
{
    std::string_view complete = "kitten";
    std::string_view a;
    std::string_view b;
    EXPECT_THROW((Util::split(complete, { a, b })), std::invalid_argument);
}

TEST(split, TooFewSeparators2)
{
    std::string_view complete = "kitten cat";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    EXPECT_THROW((Util::split(complete, { a, b, c })), std::invalid_argument);
}

TEST(split, TooFewSeparators3)
{
    std::string_view complete = "kitten cat";
    std::string_view a;
    std::string_view b;
    std::string_view c;
    std::string_view d;
    EXPECT_THROW((Util::split(complete, { a, b, c, d })), std::invalid_argument);
}
