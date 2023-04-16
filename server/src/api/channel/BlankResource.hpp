#pragma once

#include "server/Resource.hpp"

class IOContext;

namespace Api::Channel
{

/**
 * Blanks or unblanks the media (i.e: both audio and video) live input to a channel.
 *
 * The input is of type `{blank: boolean}`. The `blank` field is mandatory and specifies whether to blank (true) or
 * unblank (false) the input.
 */
class BlankResource final : public Server::Resource
{
public:
    /**
     * The request object that the resource accepts.
     */
    struct RequestObject final
    {
        /**
         * Whether to blank (true) or unblank (false) the stream.
         */
        bool blank = true;
    };

    ~BlankResource() override;

    /**
     * Constructor :)
     *
     * @param address The FFmpeg ZMQ filter's address.
     */
    BlankResource(IOContext &ioc, std::string address) : ioc(ioc), address(std::move(address)) {}

    /**
     * Do stuff in response to a (received) Request.
     *
     * This is called by postAsync, which parses its message body into a Request object, but it can also be called
     * directly.
     */
    Awaitable<void> handleRequest(const RequestObject &request);

    Awaitable<void> postAsync(Server::Response &response, Server::Request &request) override;
    size_t getMaxPostRequestLength() const noexcept override;

private:
    IOContext &ioc;
    std::string address;
};

} // namespace Api::Channel
