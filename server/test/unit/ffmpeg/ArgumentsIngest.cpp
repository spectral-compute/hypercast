#include "ffmpeg/Arguments.hpp"
#include "configuration/configuration.hpp"

#include "ArgumentsTestImpl.hpp"

#include <gtest/gtest.h>

namespace
{

TEST(FfmpegArguments, Ingest)
{
    Config::SeparatedIngestSource config = {
        .url = "rtmp://localhost:1935/",
        .arguments = {"-listen", "1"}
    };
    check({
        /* Global arguments. */
        "-loglevel", "repeat+level+info",
        "-nostdin",

        /* Input arguments. */
        // Realtime arguments.
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        // Specified arguments.
        "-listen", "1",

        // Common arguments.
        "-i", "rtmp://localhost:1935/",

        /* Output. */
        "-c:v", "copy",
        "-c:a", "copy",
        "-f", "matroska",
        "-tcp_nodelay", "1",
        "-method", "PUT",
        "http://localhost:8080/ingest/__listen__/0/stream"
    }, Ffmpeg::Arguments::ingest(config, {}, "__listen__/0"));
}

TEST(FfmpegArguments, LivestreamForIngest)
{
    Config::Channel config = {
        .source = {
            .url = "ingest_http://localhost:8080/ingest/__listen__/0"
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
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        // Common arguments.
        "-i", "http://localhost:8080/ingest/__listen__/0/stream",

        /* Filtering. */
        "-filter_complex", "nullsrc,zmq=bind_address='ipc\\:///tmp/live/abcd',nullsink; "
                           "[0:v]drawbox@vblank=thickness=fill:c=#000000:enable=0[vsrc]; "
                           "[vsrc]split=1[vin0]; "
                           "[vin0]fps=25/1,scale=1920x1080[v0]; "
                           "[0:a]volume@ablank=volume=0.0:enable=0[asrc]; [asrc]asplit=1[a0]; ",

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
        // PTS updates.
        "-stats_mux_pre:v:0", "pipe:1",
        "-stats_mux_pre_fmt:v:0", "{pts} {tb}",

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
    }, Ffmpeg::Arguments::liveStream(config, {}, "live/uid"));
}

} // namespace
