#include "coro_test.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>

#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <filesystem>
#include <span>

extern std::filesystem::path testDir;

namespace
{

constexpr uint16_t port = 12480;

/**
 * Execute the test HTTP server by using fork.
 *
 * Doing that is easier than having multiple IO contexts.
 */
class HttpServerSubprocess final
{
public:
    /**
     * Kill the HTTP server.
     */
    ~HttpServerSubprocess()
    {
        if (pid < 0) {
            return;
        }
        if (kill(pid, SIGKILL) != 0) {
            perror("Failed to kill server");
        }
    }

    /**
     * Start the HTTP server if it hasn't already been started.
     */
    Awaitable<void> operator()(IOContext &ioc)
    {
        /* Idempotency. */
        if (pid > 0) {
            co_return;
        }

        /* Fork :) */
        pid = fork();
        if (pid < 0) {
            perror("Failed to fork for test HTTP server");
        }

        /* Parent process. */
        else if (pid > 0) {
            // Wait for the child to start listening.
            for (int i = 0; i < 20; i++) {
                // Try to connect to the socket.
                try {
                    // Connect :)
                    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), port);
                    boost::asio::ip::tcp::socket socket(ioc);
                    co_await boost::asio::async_connect(socket, std::span(&endpoint, 1), boost::asio::use_awaitable);

                    // Put in a valid request, so we don't cause an exception in the HTTP server to get printed.
                    std::string_view request = "HEAD / HTTP/1.0\r\n\r\n";
                    co_await boost::asio::async_write(socket, boost::asio::const_buffer(request.data(), request.size()),
                                                      boost::asio::use_awaitable);

                    // We don't need to keep trying.
                    break;
                }
                catch (...) {}

                // Wait for a short time.
                boost::asio::steady_timer sleeper(ioc);
                sleeper.expires_from_now(std::chrono::milliseconds(100));
                co_await sleeper.async_wait(boost::asio::use_awaitable);
            }
            co_return;
        }

        /* Execve. */
        std::string testHttpServerBin = testDir / "bin" / "test-http-server";
        char *argv[] = {testHttpServerBin.data(), nullptr};
        char *envp[] = {nullptr};
        execve(testHttpServerBin.c_str(), argv, envp);

        // Error.
        perror("Failed to execve for test HTTP server");
        exit(1);
    }

private:
    pid_t pid = -1;
};
HttpServerSubprocess subprocess;

class Socket final
{
public:
    Socket(IOContext &ioc) : ioc(ioc), socket(ioc) {}

    /**
     * Read from the socket until end of file and return a byte vector.
     */
    Awaitable<std::vector<std::byte>> readAll()
    {
        /* Make sure the socket is connected. */
        co_await connect();

        /* Keep reading until end of file. */
        std::vector<std::byte> result;
        while (true) {
            std::byte buffer[4096];
            auto [e, n] = co_await socket.async_read_some(boost::asio::buffer(buffer),
                                                          boost::asio::as_tuple(boost::asio::use_awaitable));
            result.insert(result.end(), buffer, buffer + n);

            if (e == boost::asio::error::eof) {
                co_return result;
            }
            else if (e) {
                throw std::runtime_error("Error reading pipe.");
            }
        }
    }

    /**
     * Read from the socket until end of file and return a string.
     */
    Awaitable<std::string> readAllAsString()
    {
        std::vector<std::byte> data = co_await readAll();
        co_return std::string((const char *)data.data(), data.size());
    }

    /**
     * Write to the socket.
     */
    Awaitable<void> write(std::span<const std::byte> data)
    {
        co_await connect();
        co_await boost::asio::async_write(socket, boost::asio::const_buffer(data.data(), data.size()),
                                          boost::asio::use_awaitable);
    }

    /**
     * @copydoc write
     */
    Awaitable<void> write(std::string_view data)
    {
        return write(std::span((const std::byte *)data.data(), data.size()));
    }

private:
    Awaitable<void> connect()
    {
        /* Don't connect if already connected. */
        if (socket.is_open()) {
            co_return;
        }

        /* Make sure the test HTTP server is started up. */
        co_await subprocess(ioc);

        /* Connect. */
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v6(), port);
        co_await boost::asio::async_connect(socket, std::span(&endpoint, 1), boost::asio::use_awaitable);
    }

    IOContext &ioc;
    boost::asio::ip::tcp::socket socket;
};

/**
 * Check that a given string consists only of digits.
 */
bool isNumberBetween(std::string_view string, int minimum, int maximum)
{
    int value = 0;
    for (char c: string) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + c - '0';
    }
    return value >= minimum && value <= maximum;
}

/**
 * Check that a string is in a list of candidates.
 */
bool isIn(std::string_view string, std::initializer_list<std::string_view> candidates)
{
    for (std::string_view candidate: candidates) {
        if (string == candidate) {
            return true;
        }
    }
    return false;
}

/**
 * Check lines that start with Date, and remove them from the result.
 *
 * @poram firstHeadersOnly Whether to process only the first set of headers. This is an optimization. Without it, debug
 *        builds take a long time to run.
 */
std::vector<std::byte> checkAndFilterDateHeader(std::span<const std::byte> response, bool firstHeadersOnly = true)
{
    std::vector<std::byte> result;
    result.reserve(response.size());

    /* Step through each line, removing and checking any that start with Date: . */
    size_t offset = 0;
    std::vector<std::byte> line;
    for (std::byte b: response) {
        offset++;
        line.insert(line.end(), b);

        // Only the end of the line is interesting.
        if (b != (std::byte)'\n') {
            continue;
        }
        std::string_view headerLine((const char *)line.data(), line.size());

        // Check for the Date header and exclude it.
        if (headerLine.starts_with("Date: ")) {
            EXPECT_EQ(37, headerLine.size());
            EXPECT_TRUE(isIn(headerLine.substr(6, 3), { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" }))
                << headerLine;
            EXPECT_EQ(", ", headerLine.substr(9, 2)) << headerLine;
            EXPECT_TRUE(isNumberBetween(headerLine.substr(11, 2), 1, 31)) << headerLine;
            EXPECT_EQ(' ', headerLine[13]) << headerLine;
            EXPECT_TRUE(isIn(headerLine.substr(14, 3),
                             { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }))
                << headerLine;
            EXPECT_EQ(' ', headerLine[17]) << headerLine;
            EXPECT_TRUE(isNumberBetween(headerLine.substr(18, 4), 2023, 2033)) << headerLine; // Happy 10th Birthday to RISE!
            EXPECT_EQ(' ', headerLine[22]) << headerLine;
            EXPECT_TRUE(isNumberBetween(headerLine.substr(23, 2), 0, 23)) << headerLine;
            EXPECT_EQ(':', headerLine[25]) << headerLine;
            EXPECT_TRUE(isNumberBetween(headerLine.substr(26, 2), 0, 59)) << headerLine;
            EXPECT_EQ(':', headerLine[28]) << headerLine;
            EXPECT_TRUE(isNumberBetween(headerLine.substr(29, 2), 0, 60)) << headerLine; // Don't fail on leap seconds.
            EXPECT_EQ(" GMT\r\n", headerLine.substr(31)) << headerLine;
            line.clear();
            continue;
        }

        // Other lines should be inserted verbatim.
        result.insert(result.end(), line.begin(), line.end());

        // If we're not processing everything, then stop whne we get to the first empty header.
        if (firstHeadersOnly && headerLine == "\r\n") {
            line.clear();
            break;
        }

        // None of the above conditions matched, and thus didn't do this clear.
        line.clear();
    }

    /* Insert the remaining line, if any. */
    result.insert(result.end(), line.begin(), line.end());

    /* Insert the remaining data, if any. */
    result.insert(result.end(), response.begin() + offset, response.end());

    /* Done :) */
    return result;
}

/**
 * @copydoc checkAndFilterDateHeader
 */
std::string checkAndFilterDateHeader(std::string_view response, bool firstHeadersOnly = true)
{
    auto result = checkAndFilterDateHeader(std::span((const std::byte *)response.data(), response.size()),
                                           firstHeadersOnly);
    return std::string((const char *)result.data(), result.size());
}

CORO_TEST(HttpServer, Short, ioc)
{
    Socket socket(ioc);
    co_await socket.write("GET /Short HTTP/1.0\r\n"
                          "\r\n");
    EXPECT_EQ("HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Cache-Control: public, max-age=600\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 16\r\n"
              "\r\n"
              "Cats are cute :D",
              checkAndFilterDateHeader(co_await socket.readAllAsString()));
}

CORO_TEST(HttpServer, NotFound, ioc)
{
    Socket socket(ioc);
    co_await socket.write("GET /octopus HTTP/1.0\r\n"
                          "\r\n");
    EXPECT_EQ("HTTP/1.1 404 Not Found\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Cache-Control: public, max-age=600\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Length: 0\r\n"
              "\r\n",
              checkAndFilterDateHeader(co_await socket.readAllAsString()));
}

CORO_TEST(HttpServer, DotDotForbidden, ioc)
{
    Socket socket(ioc);
    co_await socket.write("GET /.. HTTP/1.0\r\n"
                          "\r\n");
    EXPECT_EQ("HTTP/1.1 403 Forbidden\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Cache-Control: public, max-age=600\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Length: 0\r\n"
              "\r\n",
              checkAndFilterDateHeader(co_await socket.readAllAsString()));
}

CORO_TEST(HttpServer, ShortChunk, ioc)
{
    Socket socket(ioc);
    co_await socket.write("GET /ShortChunk HTTP/1.0\r\n"
                          "\r\n");
    EXPECT_EQ("HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Cache-Control: public, max-age=1\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Transfer-Encoding: chunked\r\n"
              "\r\n"
              "4\r\n"
              "Cats\r\n"
              "4\r\n"
              " are\r\n"
              "5\r\n"
              " cute\r\n"
              "3\r\n"
              " :D\r\n"
              "0\r\n"
              "\r\n",
              checkAndFilterDateHeader(co_await socket.readAllAsString()));
}

CORO_TEST(HttpServer, ShortKeepAlive, ioc)
{
    Socket socket(ioc);
    co_await socket.write("GET /Short HTTP/1.1\r\n"
                          "Connection: Keep-Alive\r\n"
                          "\r\n"
                          "GET /Short HTTP/1.1\r\n"
                          "Connection: Close\r\n"
                          "\r\n");
    EXPECT_EQ("HTTP/1.1 200 OK\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Cache-Control: public, max-age=600\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 16\r\n"
              "\r\n"
              "Cats are cute :D"
              "HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Cache-Control: public, max-age=600\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 16\r\n"
              "\r\n"
              "Cats are cute :D",
              checkAndFilterDateHeader(co_await socket.readAllAsString(), false));
}

CORO_TEST(HttpServer, LengthShort, ioc)
{
    Socket socket(ioc);
    co_await socket.write("POST /Length HTTP/1.0\r\n"
                          "Content-length: 6\r\n"
                          "\r\n"
                          "Kitten");
    EXPECT_EQ("HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Length: 1\r\n"
              "\r\n"
              "6",
              checkAndFilterDateHeader(co_await socket.readAllAsString()));
}

CORO_TEST(HttpServer, LengthLarge, ioc)
{
    Socket socket(ioc);
    co_await socket.write("POST /Length HTTP/1.0\r\n"
                          "Content-length: 104857600\r\n"
                          "\r\n");
    co_await socket.write(std::vector<std::byte>(100 << 20, (std::byte)0)); // Easy test to see if the long read hangs.
    EXPECT_EQ("HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Content-Length: 9\r\n"
              "\r\n"
              "104857600",
              checkAndFilterDateHeader(co_await socket.readAllAsString()));
}

CORO_TEST(HttpServer, Echo, ioc)
{
    Socket socket(ioc);
    co_await socket.write("GET /Echo HTTP/1.0\r\n"
                          "Content-length: 6\r\n"
                          "\r\n"
                          "Kitten");
    EXPECT_EQ("HTTP/1.1 200 OK\r\n"
              "Connection: close\r\n"
              "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
              "Cache-Control: no-cache\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Transfer-Encoding: chunked\r\n"
              "\r\n"
              "6\r\n"
              "Kitten\r\n"
              "0\r\n"
              "\r\n",
              checkAndFilterDateHeader(co_await socket.readAllAsString()));
}

CORO_TEST(HttpServer, Long, ioc)
{
    /* What the header should look like. */
    std::string_view refHeader = "HTTP/1.1 200 OK\r\n"
                                 "Connection: close\r\n"
                                 "Server: Spectral Compute Ultra Low Latency Video Streamer\r\n"
                                 "Cache-Control: public, max-age=600\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Content-Length: 268435456\r\n"
                                 "\r\n";

    /* Get test data matching what LongResource from the test HTTP server returns. */
    constexpr int count = 64 << 20;
    std::vector<std::byte> refData(sizeof(int) * count);
    for (int i = 0; i < count; i++) {
        memcpy(refData.data() + i * sizeof(int), &i, sizeof(int));
    }

    /* Write the request. */
    Socket socket(ioc);
    co_await socket.write("GET /Long HTTP/1.0\r\n"
                          "\r\n");

    /* Read the response and split it into header and body. */
    std::vector<std::byte> testResult = checkAndFilterDateHeader(co_await socket.readAll());
    std::string_view testHeader((const char *)testResult.data(), std::min(refHeader.size(), testResult.size()));
    std::span testData(testResult.data() + testHeader.size(), testResult.size() - testHeader.size());

    /* Compare :) */
    EXPECT_EQ(refHeader, testHeader);
    EXPECT_EQ(refData, std::vector<std::byte>(testData.begin(), testData.end()));
}

} // namespace
