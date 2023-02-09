#include "resources/FilesystemResource.hpp"

#include "util/util.hpp"

#include "coro_test.hpp"
#include "data.hpp"
#include "TestResource.hpp"

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

} // namespace
