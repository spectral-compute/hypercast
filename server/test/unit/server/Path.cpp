#include "server/Path.hpp"

#include <gtest/gtest.h>

namespace
{

void checkABCD(const char *pathStr)
{
    Server::Path path(pathStr);
    EXPECT_FALSE(path.empty());
    ASSERT_EQ(4, path.size());
    EXPECT_EQ("alpha", path[0]);
    EXPECT_EQ("beta", path[1]);
    EXPECT_EQ("gamma", path[2]);
    EXPECT_EQ("delta", path[3]);
    EXPECT_EQ("alpha", path.front());
    EXPECT_EQ("delta", path.back());
    EXPECT_EQ("alpha/beta/gamma/delta", (std::string)path);
    EXPECT_EQ(std::filesystem::path("alpha/beta/gamma/delta"), (std::filesystem::path)path);
}

void checkOther(const char *pathStr, std::initializer_list<const char *> values)
{
    Server::Path path(pathStr);
    ASSERT_EQ(path.size(), values.size());
    for (size_t i = 0; const char *value: values) {
        EXPECT_EQ(path[i++], value);
    }
}

TEST(Path, Simple)
{
    checkABCD("alpha/beta/gamma/delta");
}

TEST(Path, FilterEmpty)
{
    checkABCD("alpha/beta/./gamma/delta");
    checkABCD("/alpha/beta/gamma/delta");
    checkABCD("alpha/beta/gamma/delta/");
    checkABCD("/alpha/beta/gamma/delta/");
}

TEST(Path, FilterDot)
{
    checkABCD("alpha/beta/./gamma/delta");
    checkABCD("./alpha/beta/gamma/delta");

    checkOther("alpha.beta", {"alpha.beta"});
    checkOther("alpha.", {"alpha."});
    checkOther(".alpha", {".alpha"});
}

TEST(Path, DotDot)
{
    EXPECT_THROW(Server::Path path(".."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("../alpha"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/.."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/../gamma"), Server::Path::Exception);

    EXPECT_THROW(Server::Path path("..."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path(".../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path(".../alpha"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/..."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/.../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/.../gamma"), Server::Path::Exception);

    EXPECT_THROW(Server::Path path("...."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("..../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("..../alpha"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/...."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/..../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/..../gamma"), Server::Path::Exception);

    EXPECT_THROW(Server::Path path("....."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("...../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("...../alpha"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/....."), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/...../"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("alpha/...../gamma"), Server::Path::Exception);

    checkOther("alpha..beta", {"alpha..beta"});
    checkOther("alpha..", {"alpha.."});
    checkOther("..alpha", {"..alpha"});
}

TEST(Path, BadChars)
{
    EXPECT_THROW(Server::Path path("\\"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("a\\b"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path(":"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path("space:time"), Server::Path::Exception);
    EXPECT_THROW(Server::Path path({"space\0time", 10}), Server::Path::Exception);
}

TEST(Path, NonAscii)
{
    EXPECT_THROW(Server::Path path("µ"), Server::Path::Exception);
}

void checkEmpty(const char *pathStr)
{
    Server::Path path(pathStr);
    EXPECT_TRUE(path.empty());
    EXPECT_EQ(0, path.size());
    EXPECT_EQ(std::filesystem::path(), (std::filesystem::path)path);
}

TEST(Path, Empty)
{
    checkEmpty("");
    checkEmpty(".");
    checkEmpty("/");
}

TEST(Path, Single)
{
    Server::Path path("cat");
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(1, path.size());
    EXPECT_EQ("cat", path[0]);
    EXPECT_EQ("cat", path.front());
    EXPECT_EQ("cat", path.back());
    EXPECT_EQ("cat", *path);
    EXPECT_EQ(std::filesystem::path("cat"), (std::filesystem::path)path);
}

TEST(Path, PopFront)
{
    Server::Path path("alpha/beta/gamma/delta");

    path.pop_front();
    EXPECT_FALSE(path.empty());
    ASSERT_EQ(3, path.size());
    EXPECT_EQ("beta", path[0]);
    EXPECT_EQ("gamma", path[1]);
    EXPECT_EQ("delta", path[2]);
    EXPECT_EQ("beta", path.front());
    EXPECT_EQ("delta", path.back());
    EXPECT_EQ(std::filesystem::path("beta/gamma/delta"), (std::filesystem::path)path);

    path.pop_front();
    EXPECT_FALSE(path.empty());
    ASSERT_EQ(2, path.size());
    EXPECT_EQ("gamma", path[0]);
    EXPECT_EQ("delta", path[1]);
    EXPECT_EQ("gamma", path.front());
    EXPECT_EQ("delta", path.back());
    EXPECT_EQ(std::filesystem::path("gamma/delta"), (std::filesystem::path)path);

    path.pop_front();
    EXPECT_FALSE(path.empty());
    ASSERT_EQ(1, path.size());
    EXPECT_EQ("delta", path[0]);
    EXPECT_EQ("delta", path.front());
    EXPECT_EQ("delta", path.back());
    EXPECT_EQ("delta", *path);
    EXPECT_EQ(std::filesystem::path("delta"), (std::filesystem::path)path);

    path.pop_front();
    EXPECT_TRUE(path.empty());
    EXPECT_EQ(0, path.size());
    EXPECT_EQ(std::filesystem::path(""), (std::filesystem::path)path);
}

TEST(Path, OperatorDivide)
{
    Server::Path a("alpha/beta");
    Server::Path b("gamma/delta");
    Server::Path c = a / b;
    EXPECT_EQ("alpha/beta/gamma/delta", (std::string)c);
    EXPECT_EQ(4, c.size());
}

TEST(Path, OperatorDivideString)
{
    Server::Path a("alpha/beta");
    Server::Path c = a / "gamma/delta";
    EXPECT_EQ("alpha/beta/gamma/delta", (std::string)c);
    EXPECT_EQ(4, c.size());
}

} // namespace
