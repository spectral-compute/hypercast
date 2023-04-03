#include "resources/FilesystemResource.hpp"

#include "util/util.hpp"

#include "coro_test.hpp"
#include "data.hpp"
#include "TestResource.hpp"

#include <filesystem>

namespace
{

CORO_TEST(FilesystemResource, Simple, ioc)
{
    auto absolutePath = getSmpteDataPath(1920, 1080, 25, 1, 48000);
    auto relativePath = std::filesystem::relative(absolutePath, getTestDataPath());

    Server::FilesystemResource resource(ioc, getTestDataPath());
    TestRequest request(Server::Path(relativePath.string()), Server::Request::Type::get);
    co_await testResource(resource, request, Util::readFile(absolutePath), "video/x-matroska");
}

CORO_TEST(FilesystemResource, NotFound, ioc)
{
    Server::FilesystemResource resource(ioc, getTestDataPath());
    TestRequest request("nonexistent.txt", Server::Request::Type::get);
    co_await testResourceError(resource, request, Server::ErrorKind::NotFound);
}

CORO_TEST(FilesystemResource, Index, ioc)
{
    auto absolutePath = getSmpteDataPath(1920, 1080, 25, 1, 48000);
    auto relativePath = std::filesystem::relative(absolutePath, getTestDataPath());

    Server::FilesystemResource resource(ioc, getTestDataPath(), relativePath);
    TestRequest request(Server::Path(""), Server::Request::Type::get);
    co_await testResource(resource, request, Util::readFile(absolutePath), "video/x-matroska");
}

CORO_TEST(FilesystemResource, Twice, ioc)
{
    auto absolutePath = getSmpteDataPath(1920, 1080, 25, 1, 48000);
    auto relativePath = std::filesystem::relative(absolutePath, getTestDataPath());

    Server::FilesystemResource resource(ioc, getTestDataPath());
    for (int i = 0; i < 2; i++) {
        TestRequest request(Server::Path(relativePath.string()), Server::Request::Type::get);
        co_await testResource(resource, request, Util::readFile(absolutePath), "video/x-matroska");
    }
}

CORO_TEST(FilesystemResource, Write, ioc)
{
    /* Get somewhere to put the test filesystem resource. */
    std::filesystem::path basePath =
        std::filesystem::temp_directory_path() / "live-video-streamer-server_test.FilesystemResourceWriteTest";
    std::filesystem::remove_all(basePath);
    std::filesystem::create_directory(basePath);

    /* Stuff to use in the test. */
    std::filesystem::path relativePath = "a/b/test.txt";
    std::vector<std::byte> refData = { (std::byte)3, (std::byte)14, (std::byte)15, (std::byte)9 };

    /* The thing to test. */
    Server::FilesystemResource resource(ioc, basePath, Server::CacheKind::fixed, false, 1024);

    /* Write the file. */
    {
        TestRequest request(Server::Path(relativePath.string()), Server::Request::Type::put, refData);
        co_await testResource(resource, request);
    }

    /* Check the write. */
    EXPECT_TRUE(std::filesystem::exists(basePath / relativePath));
    EXPECT_EQ(refData, Util::readFile(basePath / relativePath));

    /*  Read the file. */
    {
        TestRequest request(Server::Path(relativePath.string()), Server::Request::Type::get);
        co_await testResource(resource, request, refData, "application/octet-stream");
    }

    /* Clean up :) */
    std::filesystem::remove_all(basePath);
}

CORO_TEST(FilesystemResource, BadWrite, ioc)
{
    /* Get somewhere to put the test filesystem resource. */
    std::filesystem::path basePath =
        std::filesystem::temp_directory_path() / "live-video-streamer-server_test.FilesystemResourceWriteTest";
    std::filesystem::remove_all(basePath);
    std::filesystem::create_directory(basePath);

    /* Stuff to use in the test. */
    std::filesystem::path relativePath = "a/b/test.txt";
    std::vector<std::byte> refData = { (std::byte)3, (std::byte)14, (std::byte)15, (std::byte)9 };

    /* The thing to test. */
    Server::FilesystemResource resource(ioc, basePath, Server::CacheKind::fixed, false);

    /* Attempt the write, and check it's rejected. */
    {
        TestRequest request(Server::Path(relativePath.string()), Server::Request::Type::put, refData);
        co_await testResourceError(resource, request, Server::ErrorKind::UnsupportedType);
    }

    /* Check that the write did not happen. */
    EXPECT_FALSE(std::filesystem::exists(basePath / relativePath));

    /* Clean up :) */
    std::filesystem::remove_all(basePath);
}

} // namespace
