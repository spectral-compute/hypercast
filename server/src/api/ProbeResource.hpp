#pragma once

#include "server/Resource.hpp"

class IOcontext;

namespace Ffmpeg
{

class ProbeCache;

} // namespace Ffmpeg

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
 * ({
 *   video: {
 *     width: integer,
 *     height: integer,
 *     frameRate: {integer, integer}
 *   },
 *   audio: {
 *     sampleRate: integer
 *   }
 * } | null)[]
 * ```
 * and has the same length as the input (with corresponding elements). Either of `video` or `audio` may be absent. If
 * an element of the returned list is null, then the corresponding media input is not usable (e.g: it does not exist, or
 * has no usable contents). For a capture card input (such as a DeckLink), this should be interpreted as that input not
 * being connected.
 *
 * The `video.frameRate` field is a numerator/denominator pair. The other fields are self-explanatory.
 */
class ProbeResource final : public Server::Resource
{
public:
    ~ProbeResource() override;

    /**
     * Constructor :)
     *
     * @param configProbes The cache containing the media probes for the sources in the configuration. This is needed to
     *                     make sure we don't probe devices that are in use.
     */
    ProbeResource(IOContext &ioc, const Ffmpeg::ProbeCache &configProbes) : ioc(ioc), configProbes(configProbes) {}

    Awaitable<void> postAsync(Server::Response &response, Server::Request &request) override;

    size_t getMaxPostRequestLength() const noexcept override;

private:
    IOContext &ioc;
    const Ffmpeg::ProbeCache &configProbes;
};

} // namespace Api
