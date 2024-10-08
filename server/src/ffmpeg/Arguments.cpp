#include "Arguments.hpp"

#include "configuration/configuration.hpp"
#include "util/debug.hpp"

#include <algorithm>
#include <filesystem>
#include <span>

using namespace std::string_literals;

namespace
{

/* FFMPEG Arguments. */
constexpr int pipeSize = 1024; ///< Buffer size for pipes that ffmpeg uses to communicate.

/**
 * Append a vector of strings to another.
 *
 * I think this is only needed until libstdc++ catch up with C++23.
 *
 * @param dst The vector to append to.
 * @param src The vector to append.
 */
void append(std::vector<std::string> &dst, const std::vector<std::string> &src)
{
    dst.insert(dst.end(), src.begin(), src.end());
}

/**
 * Append a vector of strings to another.
 *
 * I think this is only needed until libstdc++ catch up with C++23.
 *
 * @param dst The vector to append to.
 * @param src The arguments to append.
 * @param suffix Append this to the first argument of each element of the src's outer vector.
 */
void append(std::vector<std::string> &dst, const std::vector<std::vector<std::string>> &src, const std::string &suffix)
{
    for (const std::vector<std::string> &args: src) {
        assert(!args.empty());
        dst.emplace_back(args[0] + suffix);
        dst.insert(dst.end(), args.begin() + 1, args.end());
    }
}

/**
 * Global arguments to put before everything else. These are process-global, and are for things like loglevel config.
 */
std::vector<std::string> getGlobalArgs()
{
    return {
        // Don't print "Last message repeated n times", just print the message n times (`repeat`).
        // Prefix every message with its loglevel, so we know how to shut it up (`level`).
        // Set the loglevel to `info`.
        "-loglevel", "repeat+level+info",

        // Stop ffmpeg from listening to stdin (which it does by default even if stdin isn't actually connected...).
        "-nostdin"
    };
}

/**
 * Arguments that apply to live inputs.
 */
std::vector<std::string> getRealtimeInputArgs()
{
    return {
        //"-avioflags", "direct", // TODO: Test the latency cost of this for the Decklink.
        //"-fflags", "nobuffer",
        "-rtbufsize", std::to_string(pipeSize),
        "-thread_queue_size", "0"
    };
};

/**
 * Arguments that apply to pipe and FIFO inputs.
 */
std::vector<std::string> getPipeInputArgs()
{
    std::vector<std::string> result = getRealtimeInputArgs();
    result.insert(result.end(), { "-blocksize", std::to_string(pipeSize) });
    return result;
}

/**
 * Arguments that apply to RTSP inputs.
 */
std::vector<std::string> getRtspInputArgs()
{
    std::vector<std::string> result = getRealtimeInputArgs();
    result.insert(result.end(), { "-rtsp_transport", "tcp" });
    return result;
}

/**
 * Arguments that apply to file inputs.
 */
std::vector<std::string> getFileInputArgs()
{
    return {"-re"}; // Stream the file in realtime, rather than getting ahead.
}

/**
 * See if a URL matches the regular expression `^[A-Za-z0-9]+:[/]{2}`.
 */
bool isProtocolUrl(std::string_view url)
{
    /* The protocol name. */
    while (!url.empty()) {
        char c = url[0];
        if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9')) {
            return false;
        }
        url.remove_prefix(1);
    }

    /* Make sure `://` follows next. */
    return url.starts_with("://");
}

/**
 * Get the arguments for an ffmpeg input.
 *
 * @param url The URL to give to ffmpeg, after `-i`.
 * @param arguments The arguments to give to ffmpeg before -i.
 * @param loop Whether to play the (regular file) in a loop.
 */
std::vector<std::string> getInputArgs(const std::string &url, const std::vector<std::string> &arguments, bool loop)
{
    std::vector<std::string> result;

    /* Arguments that are specific to the source kind. */
    if (url.starts_with("pipe:")) {
        append(result, getPipeInputArgs());
    }
    else if (url.starts_with("rtsp://")) {
        append(result, getRtspInputArgs());
    }
    else if (isProtocolUrl(url)) {
        // This is a guess, but the intended use of this system is realtime.
        append(result, getRealtimeInputArgs());
    }
    else if (std::filesystem::is_fifo(url)) {
        append(result, getPipeInputArgs()); // This is just a pipe that is named in the filesystem.
    }
    else if (std::filesystem::is_regular_file(url)) {
        append(result, getFileInputArgs());
        if (loop) {
            result.insert(result.end(), { "-stream_loop", "-1" });
        }
    }
    else {
        // This is a guess, but the intended use of this system is realtime.
        append(result, getRealtimeInputArgs());
    }

    /* Arguments that are explicitly provided by the source. */
    append(result, arguments);

    /* The actual input flag. */
    result.insert(result.end(), { "-i", Ffmpeg::Arguments::decodeUrl(url, "stream") });

    /* Done :) */
    return result;
}

/**
 * Quote and escape an FFMPEG filter graph argument.
 */
std::string escapeFilterArgument(std::string_view argument)
{
    std::string result;
    for (char c: argument) {
        switch (c) {
            case ':':
            case '\'':
            case '\\':
                result += '\\';
        }
        result += c;
    }
    return result;
}

/**
 * Get a filter string to print a timestamp onto the video.
 */
std::string getTimestampFilter(unsigned int width)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
            ",drawtext=text='%%{gmtime} UTC':x=%u:y=%u:fontsize=%u:borderw=%u:fontcolor=#ffffff:bordercolor=#000000:"
            "fontfile=/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            width / 40, width / 40, width / 30, width / 480);
    return buffer;
}

/**
 * Create the video filter string.
 *
 * This assumes a single video input stream.
 * The output streams are v0, v1, v2, ..., one for each quality.
 */
std::string getLiveVideoFilter(const Config::Channel &config)
{
    /* Add the ZMQ interface. */
    // The ZMQ interface is a filter that needs to be sandwiched between a source and sink of some kind. Rather than
    // inserting it arbitrarily somewhere, just create a separate null source and sink for it.
    std::string result = "nullsrc,zmq=bind_address='" + escapeFilterArgument(config.ffmpeg.filterZmq) + "',nullsink; ";

    /* Blankable input. */
    result += "[0:v]drawbox@vblank=thickness=fill:c=#000000:enable=0[vsrc]; ";

    /* Split the input. */
    result += "[vsrc]split=" + std::to_string(config.qualities.size());
    for (size_t i = 0; i < config.qualities.size(); i++) {
        result += "[vin" + std::to_string(i) + "]";
    }
    result += "; "; // Next filter.

    /* Filter each stream. */
    for (size_t i = 0; i < config.qualities.size(); i++) {
        const Config::Quality &q = config.qualities[i];
        result +=
            // Input
            "[vin" + std::to_string(i) + "]"

            // Set frame rate by dropping (or, in theory, duplicating) frames.
            "fps=" + std::to_string(q.video.frameRate.numerator) + "/" +
                     std::to_string(q.video.frameRate.denominator) + ","

            // Resample the frames to a set resolution.
            "scale=" + std::to_string(*q.video.width) + "x" + std::to_string(*q.video.height);

        // Impose the timestamp.
        if (config.source.timestamp) {
            result += getTimestampFilter(*q.video.width);
        }

        // Output name.
        result += "[v" + std::to_string(i) + "]";

        // Next filter.
        result += "; ";
    }

    /* Done :) */
    return result;
}

/**
 * Create the audio filter string.
 *
 * This assumes a single audio input stream.
 * The output streams are identical and are named a0, a1, a2, ...,
 * one for each quality.
 *
 * It is invalid to use this if the media source has no audio.
 */
std::string getLiveAudioFilter(const Config::Channel &config)
{
    std::string result;

    /* Blankable input. */
    result += "[0:a]volume@ablank=volume=0.0:enable=0[asrc]; ";

    /* Split the input. */
    result += "[asrc]asplit=" + std::to_string(config.qualities.size());
    for (size_t i = 0; i < config.qualities.size(); i++) {
        result += "[a" + std::to_string(i) + "]";
    }
    result += "; "; // Next filter.

    /* Done :) */
    return result;
}

/**
 * Get the filtering arguments.
 */
std::vector<std::string> getLiveFilterArgs(const Config::Channel &config)
{
    std::string filter = getLiveVideoFilter(config);
    /* Only add the audio filter if any of the configured qualities have audio,
     * which we expect to only happen when the media source has audio as well. */
    if (std::any_of(
        config.qualities.begin(),
        config.qualities.end(),
        [](const Config::Quality &q) { return q.audio; }
    )) {
        filter += getLiveAudioFilter(config);
    }
    return { "-filter_complex", filter };
}

/**
 * Get the arguments to build the map from filtered input to streams to encode.
 */
std::vector<std::string> getLiveMapArgs(std::span<const Config::Quality> qualities)
{
    std::vector<std::string> result;

    /* Video streams. */
    for (size_t i = 0; i < qualities.size(); i++) {
        result.insert(result.end(), { "-map", "[v" + std::to_string(i) + "]" });
    }

    for (size_t i = 0; i < qualities.size(); i++) {
        // Don't try to map audio if the quality does not have it
        if (!qualities[i].audio) {
            continue;
        }
        result.insert(result.end(), { "-map", "[a" + std::to_string(i) + "]" });
    }

    /* Done :) */
    return result;
}

/**
 * Arguments that apply to all video streams.
 *
 * @return A vector of stream-type specific arguments. The first element of the inner vector should have a stream type
 *         appended.
 */
std::vector<std::vector<std::string>> getLiveVideoStreamArgs()
{
    return { { "-pix_fmt", "yuv420p" } };
}

/**
 * Arguments that apply to all audio streams.
 *
 * @return A vector of stream-type specific arguments. The first element of the inner vector should have a stream type
 *         appended.
 */
std::vector<std::vector<std::string>> getLiveAudioStreamArgs()
{
    return { { "-ac", "1" } }; // TODO: We want to allow stereo and surround sound.
}

/**
 * Get the name of the encoder FFmpeg uses for the given codec.
 */
std::string getFfmpegCodecName(Codec::VideoCodec codec)
{
    switch (codec) {
        case Codec::VideoCodec::h264: return "h264";
        case Codec::VideoCodec::h265: return "h265";
        case Codec::VideoCodec::vp8: return "vp8";
        case Codec::VideoCodec::vp9: return "vp9";
        case Codec::VideoCodec::av1: return "av1";
    }
    unreachable();
}

/**
 * Get the name of the encoder FFmpeg uses for the given codec.
 */
std::string getFfmpegCodecName(Codec::AudioCodec codec)
{
    switch (codec) {
        case Codec::AudioCodec::none: unreachable();
        case Codec::AudioCodec::aac: return "aac";
        case Codec::AudioCodec::opus: return "opus";
    }
    unreachable();
}

/**
 * Arguments that apply to all video streams, with per stream parameters.
 *
 * @return A vector of stream-type specific arguments. The first element of the inner vector should have a stream specifier
 *         appended.
 */
std::vector<std::vector<std::string>> getLiveVideoStreamArgs(const Config::VideoQuality &q, const Config::Dash &dash)
{
    return {
        { "-c", getFfmpegCodecName(q.codec) }, // Codec.
        { "-crf", std::to_string(q.crf) }, // Constant rate factor.
        { "-minrate", std::to_string(*q.minBitrate) }, // Minimum bitrate.

        // Rate control buffer size. Used to impose the maximum bitrate.
        { "-bufsize", std::to_string(*q.bitrate * *q.rateControlBufferLength / 1000) + "k" },

        // Force IDR I-frames at segment boundaries.
        { "-forced-idr", "1" },
        { "-force_key_frames", "expr:gte(t, n_forced * " + std::to_string(dash.segmentDuration) + " / " +
                                                           std::to_string(q.gopsPerSegment * 1000) + ")" }
    };
}

/**
 * Arguments that apply to all audio streams.
 *
 * @return A vector of stream-type specific arguments. The first element of the inner vector should have a stream type
 *         appended.
 */
std::vector<std::vector<std::string>> getLiveAudioStreamArgs(const Config::AudioQuality &q)
{
    return {
        { "-c", getFfmpegCodecName(q.codec) }, // Codec.
        { "-b", std::to_string(q.bitrate) + "k" } // Bitrate.
    };
}

std::string h26xPresetToString(Config::H26xPreset preset)
{
    switch (preset) {
        case Config::H26xPreset::ultrafast: return "ultrafast";
        case Config::H26xPreset::superfast: return "superfast";
        case Config::H26xPreset::veryfast: return "veryfast";
        case Config::H26xPreset::faster: return "faster";
        case Config::H26xPreset::fast: return "fast";
        case Config::H26xPreset::medium: return "medium";
        case Config::H26xPreset::slow: return "slow";
        case Config::H26xPreset::slower: return "slower";
        case Config::H26xPreset::veryslow: return "veryslow";
        case Config::H26xPreset::placebo: return "placebo";
    }
    unreachable();
}

/**
 * Arguments that apply to h264 and h265 video streams.
 */
std::vector<std::vector<std::string>> getLiveH264StreamArgs(const Config::VideoQuality &q)
{
    return {
        { "-maxrate", std::to_string(*q.bitrate) + "k" }, // Maximum bitrate.
        { "-preset", h26xPresetToString(*q.h26xPreset) }, // Trade-off between CPU and quality/bitrate.
        { "-tune", "zerolatency" } // Minimal encoder latency.
    };
}

/**
 * Arguments that apply to VP8, VP9, and AV1 video streams.
 */
std::vector<std::vector<std::string>> getLiveVP8StreamArgs(const Config::VideoQuality &q)
{
    return {
        { "-b", std::to_string(*q.bitrate) + "k" }, // Bitrate (unfortunately, not maximum).
        { "-speed", std::to_string(q.vpXSpeed) }, // Trade-off between CPU and quality/bitrate.
        { "-deadline", "realtime" }, // Minimal encoder latency.
        { "-error-resilient", "1" }
    };
}

/**
 * Arguments that apply to VP9, and AV1 video streams.
 */
std::vector<std::vector<std::string>> getLiveVP9StreamArgs(const Config::VideoQuality &q)
{
    std::vector<std::vector<std::string>> result = getLiveVP8StreamArgs(q);
    result.insert(result.end(), {
        { "-tile-columns", "2" },
        { "-tile-rows", "2" },
        { "-row-mt", "1" },
        { "-frame-parallel", "1" }
    });
    return result;
}

/**
 * Map from video codec name to specific arguments.
 *
 * @return A vector of stream specific arguments. The first element of the inner vector should have a stream specifier
 *         appended.
 */
std::vector<std::vector<std::string>> getLiveVideoStreamArgsForCodec(const Config::VideoQuality &q)
{
    switch (q.codec) {
        case Codec::VideoCodec::h264: return getLiveH264StreamArgs(q);
        case Codec::VideoCodec::h265: return getLiveH264StreamArgs(q);
        case Codec::VideoCodec::vp8: return getLiveVP8StreamArgs(q);
        case Codec::VideoCodec::vp9: return getLiveVP9StreamArgs(q);
        case Codec::VideoCodec::av1: return getLiveVP9StreamArgs(q);
    }
    unreachable();
}

/**
 * Generate the arguments for encoding the streams.
 */
std::vector<std::string> getLiveEncoderArgs(const Config::Channel &channel)
{
    std::vector<std::string> result;

    /* Per stream-type arguments. */
    append(result, getLiveVideoStreamArgs(), ":v");
    append(result, getLiveAudioStreamArgs(), ":a");

    /* Per stream arguments for video. */
    for (size_t i = 0; i < channel.qualities.size(); i++) {
        std::string suffix = ":v:" + std::to_string(i);
        append(result, getLiveVideoStreamArgs(channel.qualities[i].video, channel.dash), suffix);
        append(result, getLiveVideoStreamArgsForCodec(channel.qualities[i].video), suffix);
    }

    /* Per stream arguments for video. */
    {
        size_t audioStreamIndex = 0;
        for (const Config::Quality &q: channel.qualities) {
            if (!q.audio) {
                continue;
            }
            std::string suffix = ":a:" + std::to_string(audioStreamIndex++);
            append(result, getLiveAudioStreamArgs(q.audio), suffix);
        }
    }

    /* Done :) */
    return result;
}

/**
 * Arguments that apply to live outputs.
 */
std::vector<std::string> getRealtimeOutputArgs()
{
    return {
        // Low latency options.
        "-flush_packets", "1",
        "-fflags", "flush_packets",

        // Flag to stop ffmpeg from emitting incorrect timestamps that lead to AV desynchronization and buffer length
        // issues.
        "-copyts"
    };
}

/**
 * Figure out whether any of the qualities have audio.
 */
bool hasAudio(std::span<const Config::Quality> qualities)
{
    for (const Config::Quality &q: qualities) {
        if (q.audio) {
            return true;
        }
    }
    return false;
}

/**
 * Format a decimal fixed-point number as a string.
 *
 * Unfortunately, -seg_duration supports decimal numbers, but not fractions.
 *
 * @param n The number, as a decimal fixed-point value.
 * @param dp The number of decimal places in the number.
 * @return The formatted string.
 */
std::string formatDecimalFixedPoint(unsigned int n, unsigned int dp)
{
    /* Figure out the multiplicative factor. */
    unsigned int factor = 1;
    for (unsigned int i = 0; i < dp; i++) {
        factor *= 10;
    }

    /* Return the integer component if there's no fractional component. */
    std::string integer = std::to_string(n / factor);
    if (n % factor == 0) {
        return integer;
    }

    /* Return a full decimal point result. */
    std::string fractional = std::to_string(n % factor);
    std::string result = std::move(integer);
    result += '.';
    for (size_t i = fractional.size(); i < dp; i++) {
        result += '0';
    }
    return result + fractional;
}

/**
 * Get the arguments to make ffmpeg write information about encoded frames to stdout.
 */
std::vector<std::string> getLiveStreamMuxInfoStdoutArgs()
{
    return {
        "-stats_mux_pre:v:0", "pipe:1",
        "-stats_mux_pre_fmt:v:0", "{pts} {tb}"
    };
}

/**
 * Arguments that apply to DASH outputs.
 */
std::vector<std::string> getDashOutputArgs(const Config::Channel &channelConfig, const Config::Network &networkConfig,
                                           std::string_view uidPath)
{
    std::vector<std::string> result = getRealtimeOutputArgs();
    result.insert(result.end(), {
        // Formatting options.
        "-f", "dash",

        // Stream selection.
        "-adaptation_sets", "id=0,streams=v"s + (hasAudio(channelConfig.qualities) ? " id=1,streams=a" : ""),

        // Emit the type of DASH manifest that allows seeking to the in-progress live-edge segment without confusion.
        "-use_timeline", "0",
        "-use_template", "1",

        // DASH segment configuration.
        "-dash_segment_type", "mp4",
        "-single_file", "0",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
        "-seg_duration", formatDecimalFixedPoint(channelConfig.dash.segmentDuration, 3),
        "-format_options", "movflags=cmaf",
        "-frag_type", "every_frame",

        // How many segments to keep/advertise.
        "-window_size", "3", // Segments the manifest advertises.
        "-extra_window_size", "2", // Segments that are kept once evicted from the manifest for lagging clients.

        // Pedantic flags we need for standards compliance.
        "-utc_timing_url", "https://time.akamai.com/?iso",
        "-target_latency", "1",

        // Low latency options.
        "-ldash", "1",
        "-streaming", "1",
        "-index_correction", "0",

        // Upload via HTTP PUT.
        "-tcp_nodelay", "1", // I'm not sure if this does anything for HTTP/DASH.
        "-method", "PUT",
        "-remove_at_exit", "1",

        // The actual manifest output.
        "http://localhost:" + std::to_string(networkConfig.port) + "/" + std::string(uidPath) + "/manifest.mpd"
    });
    return result;
}

} // namespace

Ffmpeg::Arguments::~Arguments() = default;

std::string Ffmpeg::Arguments::decodeUrl(std::string_view url, std::string_view part)
{
    return url.starts_with("ingest_http://") ?
           "http://" + std::string(url.substr(14)) + "/" + std::string(part) : std::string(url);
}

Ffmpeg::Arguments Ffmpeg::Arguments::liveStream(const Config::Channel &channelConfig,
                                                const Config::Network &networkConfig, std::string_view uidPath)
{
    Ffmpeg::Arguments result;

    result.sourceUrl = channelConfig.source.url;
    result.sourceArguments = channelConfig.source.arguments;
    result.cacheProbe = true;

    append(result.ffmpegArguments, getGlobalArgs());
    append(result.ffmpegArguments, getInputArgs(channelConfig.source.url, channelConfig.source.arguments,
                                                channelConfig.source.loop));
    append(result.ffmpegArguments, getLiveFilterArgs(channelConfig));
    append(result.ffmpegArguments, getLiveMapArgs(channelConfig.qualities));
    append(result.ffmpegArguments, getLiveEncoderArgs(channelConfig));
    append(result.ffmpegArguments, getLiveStreamMuxInfoStdoutArgs());
    append(result.ffmpegArguments, getDashOutputArgs(channelConfig, networkConfig, uidPath));

    return result;
}

Ffmpeg::Arguments Ffmpeg::Arguments::ingest(const Config::SeparatedIngestSource &ingestConfig,
                                            const Config::Network &networkConfig, std::string_view name)
{
    Arguments result;

    result.sourceUrl = ingestConfig.url;
    result.sourceArguments = ingestConfig.arguments;

    append(result.ffmpegArguments, getGlobalArgs());
    append(result.ffmpegArguments, getInputArgs(ingestConfig.url, ingestConfig.arguments, false));
    result.ffmpegArguments.insert(result.ffmpegArguments.end(), {
        // Copy the input.
        "-c:v", "copy",
        "-c:a", "copy",

        // Output format.
        "-f", "matroska",

        // Upload via HTTP PUT.
        "-tcp_nodelay", "1", // I'm not sure if this does anything for HTTP.
        "-method", "PUT",
        "http://localhost:" + std::to_string(networkConfig.port) + "/ingest/" + std::string(name) + "/stream"
    });

    return result;
}
