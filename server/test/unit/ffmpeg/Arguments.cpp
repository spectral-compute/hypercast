#include "ffmpeg/Arguments.hpp"

#include "configuration/configuration.hpp"

#include <gtest/gtest.h>

#include <span>

namespace
{

std::string toPrintable(std::span<const std::string> strings)
{
    std::string result;
    for (const std::string &string: strings) {
        result += "    " + string + "\n";
    }
    result.resize(result.size() - 1);
    return result;
}

void check(const std::vector<std::string> &ref, const Ffmpeg::Arguments &test)
{
    EXPECT_EQ(ref, test.getFfmpegArguments());
    if (ref != test.getFfmpegArguments()) {
        EXPECT_EQ(toPrintable(ref) , toPrintable(test.getFfmpegArguments()));
    }
}

} // namespace

TEST(FfmpegArguments, Simple)
{
    Config::Channel config = {
        .source = {
            .url = "rtsp://192.0.2.3"
        },
        .qualities = {
            {
                .video = {
                    .width = 1920,
                    .height = 1080,
                    .frameRate = {
                        .type = Config::FrameRate::fps,
                        .numerator = 25
                    },
                    .bitrate = 2048,
                    .minBitrate = 512,
                    .rateControlBufferLength = 333,
                    .h26xPreset = Config::H26xPreset::faster
                },
                .audio = {
                    .sampleRate = 48000
                }
            }
        },
        .ffmpeg = {
            .filterZmq = "ipc:///tmp/live/abcd"
        }
    };

    check({
        /* Global arguments. */
        "-loglevel", "repeat+level+info",
        "-nostdin",

        /* Input arguments. */
        // Realtime arguments.
        "-avioflags", "direct",
        "-fflags", "nobuffer",
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        // RTSP arguments.
        "-rtsp_transport", "tcp",

        // Common arguments.
        "-i", "rtsp://192.0.2.3",

        /* Filtering. */
        "-filter_complex", "nullsrc,zmq=bind_address='ipc\\:///tmp/live/abcd',nullsink; "
                           "[0:v]drawbox@vblank=thickness=fill:c=#000000:enable=0[vsrc]; "
                           "[vsrc]split=1[vin0]; "
                           "[vin0]fps=25/1,scale=1920x1080[v0]; "
                           "[0:a]volume@ablank=volume=0.0:enable=0[a0]",

        /* Map. */
        "-map", "[v0]", "-map", "[a0]",

        /* Per stream-type arguments. */
        "-pix_fmt:v", "yuv420p",
        "-ac:a", "1",

        /* Video stream 0 arguments. */
        // Common arguments.
        "-c:v:0", "h264",
        "-crf:v:0", "25",
        "-minrate:v:0", "512",
        "-bufsize:v:0", "681k",
        "-forced-idr:v:0", "1",
        "-force_key_frames:v:0", "expr:gte(t, n_forced * 15000 / 1000)",

        // Codec-specific arguments.
        "-maxrate:v:0", "2048k",
        "-preset:v:0", "faster",
        "-tune:v:0", "zerolatency",

        /* Audio stream 0 arguments. */
        // Common arguments.
        "-c:a:0", "aac",
        "-b:a:0", "64k",

        /* Output arguments. */
        // Realtime output arguments.
        "-flush_packets", "1",
        "-fflags", "flush_packets",
        "-copyts",

        // DASH output arguments.
        "-f", "dash",
        "-adaptation_sets", "id=0,streams=v id=1,streams=a",
        "-use_timeline", "0",
        "-use_template", "1",
        "-dash_segment_type", "mp4",
        "-single_file", "0",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
        "-seg_duration", "15",
        "-format_options", "movflags=cmaf",
        "-frag_type", "every_frame",
        "-window_size", "3",
        "-extra_window_size", "2",
        "-utc_timing_url", "https://time.akamai.com/?iso",
        "-target_latency", "1",
        "-ldash", "1",
        "-streaming", "1",
        "-index_correction", "0",
        "-tcp_nodelay", "1",
        "-method", "PUT",
        "-remove_at_exit", "1",
        "http://localhost:8080/live/uid/manifest.mpd",
    }, Ffmpeg::Arguments(config, {}, "live/uid"));
}

TEST(FfmpegArguments, Fractional)
{
    Config::Channel config = {
        .source = {
            .url = "rtsp://192.0.2.3"
        },
        .qualities = {
            {
                .video = {
                    .width = 1920,
                    .height = 1080,
                    .frameRate = {
                        .type = Config::FrameRate::fps,
                        .numerator = 25
                    },
                    .bitrate = 2048,
                    .minBitrate = 512,
                    .rateControlBufferLength = 333,
                    .h26xPreset = Config::H26xPreset::faster
                },
                .audio = {
                    .sampleRate = 48000
                }
            }
        },
        .dash = {
            .segmentDuration = 15050
        }
    };

    check({
        /* Global arguments. */
        "-loglevel", "repeat+level+info",
        "-nostdin",

        /* Input arguments. */
        // Realtime arguments.
        "-avioflags", "direct",
        "-fflags", "nobuffer",
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        // RTSP arguments.
        "-rtsp_transport", "tcp",

        // Common arguments.
        "-i", "rtsp://192.0.2.3",

        /* Filtering. */
        "-filter_complex", "nullsrc,zmq=bind_address='',nullsink; "
                           "[0:v]drawbox@vblank=thickness=fill:c=#000000:enable=0[vsrc]; "
                           "[vsrc]split=1[vin0]; "
                           "[vin0]fps=25/1,scale=1920x1080[v0]; "
                           "[0:a]volume@ablank=volume=0.0:enable=0[a0]",

        /* Map. */
        "-map", "[v0]", "-map", "[a0]",

        /* Per stream-type arguments. */
        "-pix_fmt:v", "yuv420p",
        "-ac:a", "1",

        /* Video stream 0 arguments. */
        // Common arguments.
        "-c:v:0", "h264",
        "-crf:v:0", "25",
        "-minrate:v:0", "512",
        "-bufsize:v:0", "681k",
        "-forced-idr:v:0", "1",
        "-force_key_frames:v:0", "expr:gte(t, n_forced * 15050 / 1000)",

        // Codec-specific arguments.
        "-maxrate:v:0", "2048k",
        "-preset:v:0", "faster",
        "-tune:v:0", "zerolatency",

        /* Audio stream 0 arguments. */
        // Common arguments.
        "-c:a:0", "aac",
        "-b:a:0", "64k",

        /* Output arguments. */
        // Realtime output arguments.
        "-flush_packets", "1",
        "-fflags", "flush_packets",
        "-copyts",

        // DASH output arguments.
        "-f", "dash",
        "-adaptation_sets", "id=0,streams=v id=1,streams=a",
        "-use_timeline", "0",
        "-use_template", "1",
        "-dash_segment_type", "mp4",
        "-single_file", "0",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
        "-seg_duration", "15.050",
        "-format_options", "movflags=cmaf",
        "-frag_type", "every_frame",
        "-window_size", "3",
        "-extra_window_size", "2",
        "-utc_timing_url", "https://time.akamai.com/?iso",
        "-target_latency", "1",
        "-ldash", "1",
        "-streaming", "1",
        "-index_correction", "0",
        "-tcp_nodelay", "1",
        "-method", "PUT",
        "-remove_at_exit", "1",
        "http://localhost:8080/live/uid/manifest.mpd",
    }, Ffmpeg::Arguments(config, {}, "live/uid"));
}

TEST(FfmpegArguments, Source)
{
    Config::Channel config = {
        .source = {
            .url = "rtsp://192.0.2.3",
            .arguments = { "-ss", "10" }
        },
        .qualities = {
            {
                .video = {
                    .width = 1920,
                    .height = 1080,
                    .frameRate = {
                        .type = Config::FrameRate::fps,
                        .numerator = 25
                    },
                    .bitrate = 2048,
                    .minBitrate = 512,
                    .rateControlBufferLength = 333,
                    .h26xPreset = Config::H26xPreset::faster
                },
                .audio = {
                    .sampleRate = 48000
                }
            }
        },
        .dash = {
            .segmentDuration = 15050
        }
    };
    Ffmpeg::Arguments test = Ffmpeg::Arguments(config, {}, "live/uid");

    EXPECT_EQ("rtsp://192.0.2.3", test.getSourceUrl());
    EXPECT_EQ(std::vector<std::string>({ "-ss", "10" }), test.getSourceArguments());
}

TEST(FfmpegArguments, TwoVideoStreams)
{
    Config::Channel config = {
        .source = {
            .url = "rtsp://192.0.2.3"
        },
        .qualities = {
            {
                .video = {
                    .width = 1920,
                    .height = 1080,
                    .frameRate = {
                        .type = Config::FrameRate::fps,
                        .numerator = 25
                    },
                    .bitrate = 2048,
                    .minBitrate = 512,
                    .rateControlBufferLength = 333,
                    .h26xPreset = Config::H26xPreset::faster
                },
                .audio = {
                    .sampleRate = 48000
                }
            },
            {
                .video = {
                    .width = 1280,
                    .height = 720,
                    .frameRate = {
                        .type = Config::FrameRate::fps,
                        .numerator = 25
                    },
                    .bitrate = 1024,
                    .minBitrate = 256,
                    .rateControlBufferLength = 333,
                    .h26xPreset = Config::H26xPreset::faster
                },
                .audio = {
                    .sampleRate = 48000
                }
            }
        },
        .dash = {
            .segmentDuration = 15500
        }
    };

    check({
        /* Global arguments. */
        "-loglevel", "repeat+level+info",
        "-nostdin",

        /* Input arguments. */
        // Realtime arguments.
        "-avioflags", "direct",
        "-fflags", "nobuffer",
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        // RTSP arguments.
        "-rtsp_transport", "tcp",

        // Common arguments.
        "-i", "rtsp://192.0.2.3",

        /* Filtering. */
        "-filter_complex", "nullsrc,zmq=bind_address='',nullsink; "
                           "[0:v]drawbox@vblank=thickness=fill:c=#000000:enable=0[vsrc]; "
                           "[vsrc]split=2[vin0][vin1]; "
                           "[vin0]fps=25/1,scale=1920x1080[v0]; "
                           "[vin1]fps=25/1,scale=1280x720[v1]; "
                           "[0:a]volume@ablank=volume=0.0:enable=0[a0]",

        /* Map. */
        "-map", "[v0]", "-map", "[v1]", "-map", "[a0]", "-map", "[a0]",

        /* Per stream-type arguments. */
        "-pix_fmt:v", "yuv420p",
        "-ac:a", "1",

        /* Video stream 0 arguments. */
        // Common arguments.
        "-c:v:0", "h264",
        "-crf:v:0", "25",
        "-minrate:v:0", "512",
        "-bufsize:v:0", "681k",
        "-forced-idr:v:0", "1",
        "-force_key_frames:v:0", "expr:gte(t, n_forced * 15500 / 1000)",

        // Codec-specific arguments.
        "-maxrate:v:0", "2048k",
        "-preset:v:0", "faster",
        "-tune:v:0", "zerolatency",

        /* Video stream 1 arguments. */
        // Common arguments.
        "-c:v:1", "h264",
        "-crf:v:1", "25",
        "-minrate:v:1", "256",
        "-bufsize:v:1", "340k",
        "-forced-idr:v:1", "1",
        "-force_key_frames:v:1", "expr:gte(t, n_forced * 15500 / 1000)",

        // First codec-specific arguments.
        "-maxrate:v:1", "1024k",
        "-preset:v:1", "faster",
        "-tune:v:1", "zerolatency",

        /* Audio stream 0 arguments. */
        // Common arguments.
        "-c:a:0", "aac",
        "-b:a:0", "64k",

        /* Audio stream 1 arguments. */
        // Common arguments.
        "-c:a:1", "aac",
        "-b:a:1", "64k",

        /* Output arguments. */
        // Realtime output arguments.
        "-flush_packets", "1",
        "-fflags", "flush_packets",
        "-copyts",

        // DASH output arguments.
        "-f", "dash",
        "-adaptation_sets", "id=0,streams=v id=1,streams=a",
        "-use_timeline", "0",
        "-use_template", "1",
        "-dash_segment_type", "mp4",
        "-single_file", "0",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
        "-seg_duration", "15.500",
        "-format_options", "movflags=cmaf",
        "-frag_type", "every_frame",
        "-window_size", "3",
        "-extra_window_size", "2",
        "-utc_timing_url", "https://time.akamai.com/?iso",
        "-target_latency", "1",
        "-ldash", "1",
        "-streaming", "1",
        "-index_correction", "0",
        "-tcp_nodelay", "1",
        "-method", "PUT",
        "-remove_at_exit", "1",
        "http://localhost:8080/live/uid/manifest.mpd",
    }, Ffmpeg::Arguments(config, {}, "live/uid"));
}
