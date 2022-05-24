import {expect, test} from "@jest/globals";
import {applyDefaultConfig} from "../src/Config/Config";
import {getTranscoderArgs} from "../src/Ffmpeg";

test("1 video", (): void => {
    const config = applyDefaultConfig({});
    const args = getTranscoderArgs(0, config, "pipe:3", "uid");
    expect(args).toStrictEqual([
        /* globalArgs */
        "-loglevel", "repeat+level+info",
        "-nostdin",

        /* realtimeInputArgs */
        "-avioflags", "direct",
        "-fflags", "nobuffer",
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        /* pipeInputArgs */
        "-blocksize", "1024",

        /* getExternalSourceArgs */
        "-i", "pipe:3",

        /* getFilteringArgs */
        "-filter_complex", "[0:v]split=1[vin0]; [vin0]fps=30000/1001,scale=1920x1080[v0]",

        /* Output map in getTranscoderArgs(). */
        "-map", "[v0]",

        /* videoArgs */
        "-pix_fmt:v", "yuv420p",

        /* audioArgs */
        "-ac:a", "1",

        /* getEncodeArgs stream-specific arguments */
        "-c:v:0", "h264",
        "-crf:v:0", "25",
        "-maxrate:v:0", "3000k",
        "-bufsize:v:0", "3000k",
        "-g:v:0", "450",
        "-force_key_frames:v:0", "expr:eq(mod(n, 450), 0)",

        // Codec-specific options.
        "-preset:v:0", "faster",

        /* h264Args */
        "-tune:v:0", "zerolatency",

        /* getDashOutputArgs */
        "-seg_duration", "15.015",
        "-target_latency", "0.5",
        "-adaptation_sets", "id=0,streams=v",

        /* outputArgs */
        "-flush_packets", "1",
        "-fflags", "flush_packets", "-copyts",

        /* dashArgs */
        "-f", "dash",
        "-use_timeline", "0",
        "-use_template", "1",
        "-dash_segment_type", "mp4",
        "-single_file", "0",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
        "-format_options", "movflags=cmaf",
        "-frag_type", "every_frame",
        "-window_size", "3",
        "-extra_window_size", "2",
        "-utc_timing_url", "https://time.akamai.com/?iso",
        "-ldash", "1",
        "-streaming", "1",
        "-index_correction", "0",
        "-tcp_nodelay", "1",
        "-method", "PUT",
        "-remove_at_exit", "1",
        "http://localhost:8080/live/uid/angle-0/manifest.mpd"
    ]);
});

test("2 videos", (): void => {
    const config = applyDefaultConfig({
        video: {
            configs: [
                {
                    bitrate: 2000
                },
                {
                    bitrate: 1000,
                    height: 720,
                    width: 1280
                }
            ]
        }
    });
    const args = getTranscoderArgs(0, config, "pipe:3", "uid");
    expect(args).toStrictEqual([
        /* globalArgs */
        "-loglevel", "repeat+level+info",
        "-nostdin",

        /* realtimeInputArgs */
        "-avioflags", "direct",
        "-fflags", "nobuffer",
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        /* pipeInputArgs */
        "-blocksize", "1024",

        /* getExternalSourceArgs */
        "-i", "pipe:3",

        /* getFilteringArgs */
        "-filter_complex", "[0:v]split=2[vin0][vin1]; [vin0]fps=30000/1001,scale=1920x1080[v0]; " +
        "[vin1]fps=30000/1001,scale=1280x720[v1]",

        /* Output map in getTranscoderArgs(). */
        "-map", "[v0]",
        "-map", "[v1]",

        /* videoArgs */
        "-pix_fmt:v", "yuv420p",

        /* audioArgs */
        "-ac:a", "1",

        /* getEncodeArgs stream-specific arguments for stream 0 */
        "-c:v:0", "h264",
        "-crf:v:0", "25",
        "-maxrate:v:0", "2000k",
        "-bufsize:v:0", "2000k",
        "-g:v:0", "450",
        "-force_key_frames:v:0", "expr:eq(mod(n, 450), 0)",

        // Codec-specific options.
        "-preset:v:0", "faster",

        /* h264Args for stream 0 */
        "-tune:v:0", "zerolatency",

        /* getEncodeArgs stream-specific arguments for stream 1 */
        "-c:v:1", "h264",
        "-crf:v:1", "25",
        "-maxrate:v:1", "1000k",
        "-bufsize:v:1", "1000k",
        "-g:v:1", "450",
        "-force_key_frames:v:1", "expr:eq(mod(n, 450), 0)",

        // Codec-specific options.
        "-preset:v:1", "faster",

        /* h264Args for stream 1 */
        "-tune:v:1", "zerolatency",

        /* getDashOutputArgs */
        "-seg_duration", "15.015",
        "-target_latency", "0.5",
        "-adaptation_sets", "id=0,streams=v",

        /* outputArgs */
        "-flush_packets", "1",
        "-fflags", "flush_packets", "-copyts",

        /* dashArgs */
        "-f", "dash",
        "-use_timeline", "0",
        "-use_template", "1",
        "-dash_segment_type", "mp4",
        "-single_file", "0",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
        "-format_options", "movflags=cmaf",
        "-frag_type", "every_frame",
        "-window_size", "3",
        "-extra_window_size", "2",
        "-utc_timing_url", "https://time.akamai.com/?iso",
        "-ldash", "1",
        "-streaming", "1",
        "-index_correction", "0",
        "-tcp_nodelay", "1",
        "-method", "PUT",
        "-remove_at_exit", "1",
        "http://localhost:8080/live/uid/angle-0/manifest.mpd"
    ]);
});

test("1 video with audio", (): void => {
    const config = applyDefaultConfig({
        audio: {
            configs: [
                {
                    bitrate: 48
                }
            ]
        }
    });
    const args = getTranscoderArgs(0, config, "pipe:3", "uid");
    expect(args).toStrictEqual([
        /* globalArgs */
        "-loglevel", "repeat+level+info",
        "-nostdin",

        /* realtimeInputArgs */
        "-avioflags", "direct",
        "-fflags", "nobuffer",
        "-rtbufsize", "1024",
        "-thread_queue_size", "0",

        /* pipeInputArgs */
        "-blocksize", "1024",

        /* getExternalSourceArgs */
        "-i", "pipe:3",

        /* getFilteringArgs */
        "-filter_complex", "[0:v]split=1[vin0]; [vin0]fps=30000/1001,scale=1920x1080[v0]",

        /* Output map in getTranscoderArgs(). */
        "-map", "[v0]",
        "-map", "0:a",

        /* videoArgs */
        "-pix_fmt:v", "yuv420p",

        /* audioArgs */
        "-ac:a", "1",

        /* getEncodeArgs stream-specific arguments for video stream */
        "-c:v:0", "h264",
        "-crf:v:0", "25",
        "-maxrate:v:0", "3000k",
        "-bufsize:v:0", "3000k",
        "-g:v:0", "450",
        "-force_key_frames:v:0", "expr:eq(mod(n, 450), 0)",

        // Codec-specific options.
        "-preset:v:0", "faster",

        /* h264Args */
        "-tune:v:0", "zerolatency",

        /* getEncodeArgs stream-specific arguments for audio stream */
        "-c:a:0", "aac",
        "-b:a:0", "48k",

        /* getDashOutputArgs */
        "-seg_duration", "15.015",
        "-target_latency", "0.5",
        "-adaptation_sets", "id=0,streams=v id=1,streams=a",

        /* outputArgs */
        "-flush_packets", "1",
        "-fflags", "flush_packets", "-copyts",

        /* dashArgs */
        "-f", "dash",
        "-use_timeline", "0",
        "-use_template", "1",
        "-dash_segment_type", "mp4",
        "-single_file", "0",
        "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
        "-format_options", "movflags=cmaf",
        "-frag_type", "every_frame",
        "-window_size", "3",
        "-extra_window_size", "2",
        "-utc_timing_url", "https://time.akamai.com/?iso",
        "-ldash", "1",
        "-streaming", "1",
        "-index_correction", "0",
        "-tcp_nodelay", "1",
        "-method", "PUT",
        "-remove_at_exit", "1",
        "http://localhost:8080/live/uid/angle-0/manifest.mpd"
    ]);
});
