#pragma once

#include "server/Resource.hpp"

class IOcontext;

namespace Api
{

/**
 * Probes media sources (e.g: with ffprobe) to determine their characteristics.
 *
 * The input is of type `{url: string, arguments: string[]}[]`. The `url` field is mandatory and specifies the input
 * argument to `ffprobe`. The `arguments` field is optional and specifies additional arguments to insert before the
 * input argument.
 *
 * The output is of type:
 * ```
 * {
 *   video: {
 *     width: integer,
 *     height: integer,
 *     frameRate: {integer, integer}
 *   },
 *   audio: {
 *     sampleRate: integer
 *   }
 * }[]
 * ```
 * and has the same length as the input (with corresponding elements). Either of `video` or `audio` may be absent. If
 * both are absent, then the media file is not usable. For a capture card input (such as a DeckLink), this should be
 * interpreted as that input not being connected.
 *
 * The `video.frameRate` field is a numerator/denominator pair. The other fields are self-explanatory.
 */
class ProbeResource final : public Server::Resource
{
public:
    ~ProbeResource() override;
    ProbeResource(IOContext &ioc) : ioc(ioc) {}

    Awaitable<void> getAsync(Server::Response &response, Server::Request &request) override;

    size_t getMaxGetRequestLength() const noexcept override;

private:
    class IOContext &ioc;
};

} // namespace Api
