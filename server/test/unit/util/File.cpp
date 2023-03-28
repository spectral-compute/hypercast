#include "util/File.hpp"

#include "util/util.hpp"

#include "coro_test.hpp"
#include "data.hpp"

CORO_TEST(File, ReadAll, ioc)
{
    std::filesystem::path path = getSmpteDataPath(1920, 1080, 25, 1, 48000);

    /* Get the reference. */
    std::vector<std::byte> ref = Util::readFile(path);

    /* Check that the read happens correctly. */
    Util::File file(ioc, path);
    EXPECT_EQ(ref, co_await file.readAll());
}

CORO_TEST(File, Seek, ioc)
{
    std::filesystem::path path = getSmpteDataPath(1920, 1080, 25, 1, 48000);

    /* Get the reference. */
    std::vector<std::byte> fileContents = Util::readFile(path);

    size_t offset = fileContents.size() / 2;
    std::vector<std::byte> ref(fileContents.begin() + offset, fileContents.end());

    /* Check that the read happens correctly. */
    Util::File file(ioc, path);
    file.seek(offset);
    EXPECT_EQ(ref, co_await file.readAll());
}

CORO_TEST(File, Write, ioc)
{
    std::filesystem::path path = "/tmp/live-video-streamer-server_test.FileWrite";

    /* Get the reference. */
    std::vector<std::byte> ref(1 << 20);
    for (size_t i = 0; i < ref.size(); i++) {
        ref[i] = (std::byte)i;
    }

    /* Write the data to a file. */
    {
        Util::File file(ioc, path, true, false);
        co_await file.write(ref);
    }

    /* Check that the write happened correctly. */
    EXPECT_EQ(ref, Util::readFile(path));
}

CORO_TEST(File, ReadWrite, ioc)
{
    std::filesystem::path path = "/tmp/live-video-streamer-server_test.FileReadWrite";

    /* Generate reference data. */
    std::vector<std::byte> allData(1 << 20);
    for (size_t i = 0; i < allData.size(); i++) {
        allData[i] = (std::byte)i;
    }

    {
        /* Open the file as read/write. */
        Util::File file(ioc, path, true);

        /* Write some data. */
        co_await file.write(allData);

        /* Read some data. */
        {
            size_t offset = allData.size() / 2;
            size_t size = 1 << 10;
            std::vector<std::byte> ref(allData.begin() + offset, allData.begin() + offset + size);

            file.seek(offset);
            EXPECT_EQ(ref, co_await file.readExact(size));
        }

        /* Read lots of data. */
        {
            size_t offset = allData.size() / 4;
            size_t size = 1 << 19;
            std::vector<std::byte> ref(allData.begin() + offset, allData.begin() + offset + size);

            file.seek(offset);
            EXPECT_EQ(ref, co_await file.readExact(size));
        }

        /* Write more data. */
        file.seekToEnd();
        co_await file.write(allData);
    }

    /* See if we got the last write right. */
    {
        std::vector<std::byte> ref = allData;
        ref.insert(ref.end(), allData.begin(), allData.end());
        EXPECT_EQ(ref, Util::readFile(path));
    }
}
