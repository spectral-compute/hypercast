#include "dash/InterleaveResource.hpp"

#include "coro_test.hpp"
#include "log/MemoryLog.hpp"
#include "resources/TestResource.hpp"

#include <random>

namespace
{

std::vector<std::byte> getVector(std::initializer_list<unsigned char> data)
{
    std::vector<std::byte> result(data.size());
    memcpy(result.data(), data.begin(), data.size());
    return result;
}

std::vector<std::byte> getChunkLength1(const std::vector<std::byte> &data, unsigned int stream = 0)
{
    EXPECT_GT(1 << 8, data.size());
    EXPECT_GT(32, stream);
    std::vector<std::byte> result(data.size() + 2);
    result[0] = (std::byte)stream;
    result[1] = (std::byte)data.size();
    memcpy(result.data() + 2, data.data(), data.size());
    return result;
}

std::vector<std::byte> getChunkLength2(const std::vector<std::byte> &data, unsigned int stream = 0)
{
    EXPECT_GT(1 << 16, data.size());
    EXPECT_GT(31, stream);
    std::vector<std::byte> result(data.size() + 3);
    result[0] = (std::byte)stream | (std::byte)(1 << 6);
    result[1] = (std::byte)(data.size() & 0xFF);
    result[2] = (std::byte)(data.size() >> 8);
    memcpy(result.data() + 3, data.data(), data.size());
    return result;
}

std::vector<std::byte> getChunkLength4(const std::vector<std::byte> &data, unsigned int stream = 0)
{
    EXPECT_GT(1zu << 32, data.size());
    EXPECT_GT(31, stream);
    std::vector<std::byte> result(data.size() + 5);
    result[0] = (std::byte)stream | (std::byte)(2 << 6);
    result[1] = (std::byte)(data.size() & 0xFF);
    result[2] = (std::byte)(data.size() >> 8);
    result[3] = (std::byte)(data.size() >> 16);
    result[4] = (std::byte)(data.size() >> 32);
    memcpy(result.data() + 5, data.data(), data.size());
    return result;
}

std::vector<std::byte> getShortData()
{
    return getVector({0x5A, 0xA5, 0x55, 0xAA, 0x33, 0xCC});
}

std::vector<std::byte> getData(size_t size, int seed = 0)
{
    std::uniform_int_distribution d(0, 255);
    std::minstd_rand e;
    e.seed(seed);
    std::vector<std::byte> result(size);
    for (size_t i = 0; i < size; i++) {
        result[i] = (std::byte)d(e);
    }
    return result;
}

CORO_TEST(InterleaveResource, SimpleLength1, ioc)
{
    Log::MemoryLog log(ioc, Log::Level::fatal, false);
    Dash::InterleaveResource resource(ioc, log, 1);
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData(getShortData(), 0); // A data chunk.
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData({}, 0); // End of stream.
    EXPECT_TRUE(resource.hasEnded());

    TestRequest request;
    co_await testResource(resource, request, {{
        getChunkLength1(getShortData()),
        getChunkLength1({})
    }});
}

CORO_TEST(InterleaveResource, SimpleLength2, ioc)
{
    Log::MemoryLog log(ioc, Log::Level::fatal, false);
    Dash::InterleaveResource resource(ioc, log, 1);
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData(getData(3 << 8), 0); // A data chunk.
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData({}, 0); // End of stream.
    EXPECT_TRUE(resource.hasEnded());

    TestRequest request;
    co_await testResource(resource, request, {{
        getChunkLength2(getData(3 << 8)),
        getChunkLength1({})
    }});
}

CORO_TEST(InterleaveResource, SimpleLength4, ioc)
{
    Log::MemoryLog log(ioc, Log::Level::fatal, false);
    Dash::InterleaveResource resource(ioc, log, 1);
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData(getData(3 << 16), 0); // A data chunk.
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData({}, 0); // End of stream.
    EXPECT_TRUE(resource.hasEnded());

    TestRequest request;
    co_await testResource(resource, request, {{
        getChunkLength4(getData(3 << 16)),
        getChunkLength1({})
    }});
}

CORO_TEST(InterleaveResource, TwoStreams, ioc)
{
    Log::MemoryLog log(ioc, Log::Level::fatal, false);
    Dash::InterleaveResource resource(ioc, log, 2);
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData(getShortData(), 0); // A data chunk.
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData(getShortData(), 1); // A data chunk.
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData({}, 1); // End of stream.
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData({}, 0); // End of stream.
    EXPECT_TRUE(resource.hasEnded());

    TestRequest request;
    co_await testResource(resource, request, {{
        getChunkLength1(getShortData(), 0),
        getChunkLength1(getShortData(), 1),
        getChunkLength1({}, 1),
        getChunkLength1({}, 0)
    }});
}

CORO_TEST(InterleaveResource, ControlChunk, ioc)
{
    Log::MemoryLog log(ioc, Log::Level::fatal, false);
    Dash::InterleaveResource resource(ioc, log, 1);
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData(getShortData(), 0); // A data chunk.
    EXPECT_FALSE(resource.hasEnded());

    resource.addControlChunk(Dash::ControlChunkType::discard, getShortData()); // A control chunk.
    EXPECT_FALSE(resource.hasEnded());

    resource.addStreamData({}, 0); // End of stream.
    EXPECT_TRUE(resource.hasEnded());

    /* Test the control chunk as if it was any other chunk, but with the control chunk header and index. */
    std::vector<std::byte> controlChunkRef = getShortData();
    controlChunkRef.insert(controlChunkRef.begin(), (std::byte)Dash::ControlChunkType::discard);

    TestRequest request;
    co_await testResource(resource, request, {{
        getChunkLength1(getShortData()),
        getChunkLength1(controlChunkRef, Dash::InterleaveResource::maxStreams),
        getChunkLength1({})
    }});
}

} // namespace
