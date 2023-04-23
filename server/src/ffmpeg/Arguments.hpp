#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Config
{

struct Channel;
struct Network;

} // namespace Config

namespace Ffmpeg
{

/**
 * Represents arguments to give to ffmpeg.
 *
 * This object should be given to FfmpegProcess.
 */
class Arguments final
{
public:
    /**
     * Generate the arguments for starting a live stream with ffmpeg.
     *
     * @param channelConfig The configuration object for the specific channel.
     * @param networkConfig The channel configuration object for the network.
     * @param uidPath The base path for the DASH streams.
     */
    static Arguments liveStream(const Config::Channel &channelConfig, const Config::Network &networkConfig,
                                std::string_view uidPath);

    ~Arguments();

    // It's not that we can't copy. Just that we shouldn't.
    Arguments(const Arguments &) = delete;
    Arguments(Arguments &&other) = default; // Move construction is fine :)
    Arguments &operator=(const Arguments &) = delete;
    Arguments &operator=(Arguments &&other) = delete; // Move assignment would be OK, but also would be unused.

    /**
     * Get the arguments that should give given to the ffmpeg process.
     */
    const std::vector<std::string> &getFfmpegArguments() const
    {
        return ffmpegArguments;
    }

    /**
     * Get the media source URL for these arguments.
     */
    const std::string &getSourceUrl() const
    {
        return sourceUrl;
    }

    /**
     * Get the media source arguments (i.e: those to give to ffmpeg before -i) for these arguments.
     */
    const std::vector<std::string> &getSourceArguments() const
    {
        return sourceArguments;
    }

private:
    Arguments() = default;

    std::vector<std::string> ffmpegArguments;
    std::string sourceUrl;
    std::vector<std::string> sourceArguments;
};

} // namespace Ffmpeg
