#include "zmqsend.hpp"

#include "util/asio.hpp"
#include "util/Mutex.hpp"
#include "util/subprocess.hpp"

#include <cassert>
#include <map>
#include <stdexcept>
#include <string>

namespace
{

/**
 * Represents mutually exclusive access to an FFmpeg filter graph's ZMQ node (at least for this process).
 */
class ZmqClient final
{
public:
    /**
     * Garbage collect the mutual-exclusion stuff.
     */
    ~ZmqClient()
    {
        /* Don't do the cache management stuff if this is the remnant of a moved object. */
        if (!lock) {
            return;
        }

        /* Unlock the mutex and remove our reference to it. */
        lock.reset();
        mutex.reset();

        /* Remove the mutex's entry in the mutex map if nothing is using it. */
        auto it = zmqClients.find(address);
        assert(it != zmqClients.end());
        if (it->second.expired()) {
            zmqClients.erase(it);
        }
    }

    /**
     * Get an object to send commands to an FFmpeg filter graph via ZMQ without interference from anything else.
     */
    static Awaitable<ZmqClient> getForAddress(IOContext &ioc, std::string address)
    {
        /* Find or create the mutex. */
        std::shared_ptr<Mutex> mutex;
        auto it = zmqClients.find(address);
        if (it == zmqClients.end()) {
            mutex = std::make_shared<Mutex>(ioc);
            zmqClients[address] = mutex;
        }
        else {
            assert(!it->second.expired());
            mutex = it->second.lock();
        }

        /* Create a ZMQ client object. */
        co_return ZmqClient(ioc, co_await mutex->lockGuard(), std::move(mutex), std::move(address));
    }

    ZmqClient(ZmqClient &&) = default;

    /**
     * Send a command to the ZMQ server.
     */
    Awaitable<void> operator()(const Ffmpeg::ZmqCommand &command)
    {
        /* Format the message. */
        std::string message;
        message.insert(message.end(), command.target.begin(), command.target.end());
        message += " ";
        message.insert(message.end(), command.command.begin(), command.command.end());
        if (!command.argument.empty()) {
            message += " ";
            message.insert(message.end(), command.argument.begin(), command.argument.end());
        }

        /* Send the message. */
        std::string result = co_await Subprocess::getStdout(ioc, "zmqsend", { "-b", address }, message);

        // Check that there's no error.
        if (result.size() < 2 || !result.starts_with("0 ")) {
            throw std::runtime_error("FFmpeg ZMQ command failed: " + result);
        }
    }

private:
    explicit ZmqClient(IOContext &ioc, Mutex::LockGuard lock, std::shared_ptr<Mutex> mutex, std::string address) :
        ioc(ioc), mutex(std::move(mutex)), lock(std::make_unique<Mutex::LockGuard>(std::move(lock))),
        address(std::move(address))
    {
    }

    static inline std::map<std::string, std::weak_ptr<Mutex>> zmqClients;

    IOContext &ioc;
    std::shared_ptr<Mutex> mutex; ///< Keeps the mutex alive so it can be unlocked.
    std::unique_ptr<Mutex::LockGuard> lock; ///< Keeps the mutex locked.
    std::string address;
};

} // namespace

Awaitable<void> Ffmpeg::zmqsend(IOContext &ioc, std::string_view address, std::span<const ZmqCommand> commands,
                                bool sequential)
{
    ZmqClient zmqClient = co_await ZmqClient::getForAddress(ioc, (std::string)address);

    if (sequential) {
        for (const ZmqCommand &command: commands) {
            co_await zmqClient(command);
        }
    }
    else {
        std::vector<Awaitable<void>> awaitables;
        awaitables.reserve(commands.size());

        for (const ZmqCommand &command: commands) {
            awaitables.emplace_back(zmqClient(command));
        }
        co_await awaitTree(awaitables);
    }
}

Awaitable<void> Ffmpeg::zmqsend(IOContext &ioc, std::string_view address, std::initializer_list<ZmqCommand> commands,
                                bool sequential)
{
    co_await zmqsend(ioc, address, std::span(&*commands.begin(), commands.size()), sequential);
}
