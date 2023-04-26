#pragma once

#include "server/Resource.hpp"

class IOContext;

namespace Dash
{

class DashResources;

} // namespace Dash

namespace Ffmpeg
{

class Process;

} // namespace Ffmpeg

namespace Api::Channel
{

class BlankResource;

/**
 * Requests the client to display interjections.
 */
class InterjectionResource final : public Server::Resource
{
public:
    ~InterjectionResource() override;

    /**
     * Constructor :)
     *
     * @param channel Where to send client messages to.
     * @param ffmpegProcess The ffmpeg process to get timestamps from.
     * @param blankResource The resource for blanking the stream.
     */
    InterjectionResource(IOContext &ioc, Dash::DashResources &channel, const Ffmpeg::Process &ffmpegProcess,
                         BlankResource &blankResource) :
        ioc(ioc), channel(channel), ffmpegProcess(ffmpegProcess), blankResource(blankResource)
    {
    }

    Awaitable<void> postAsync(Server::Response &response, Server::Request &request) override;
    size_t getMaxPostRequestLength() const noexcept override;

private:
    IOContext &ioc;
    Dash::DashResources &channel;
    const Ffmpeg::Process &ffmpegProcess;
    BlankResource &blankResource;
};

} // namespace Api::Channel
