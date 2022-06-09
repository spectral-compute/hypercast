import assert = require("assert");
import * as child_process from "child_process";
import * as fs from "fs";
import * as process from "process";
import * as stream from "stream";
import Timeout = NodeJS.Timeout;
import {AudioConfig, CodecOptions, Config, computeSegmentDuration, substituteManifestPattern,
        VideoConfig} from "./Config/Config";
import {Logger} from "./Log";

/* FFMPEG Arguments. */
const pipeSize = 1024; // Buffer size for pipes that ffmpeg uses to communicate.

// Global arguments to put before everything else. These are process-global, and are for things like loglevel config.
const globalArgs = [
    // Don't print "Last message repeated n times", just print the message n times. Makes the results in graylog more
    // helpful. (`repeat`).
    // Prefix every message with its loglevel, so we know how to shut it up (`level`).
    // Set the loglevel to `info`.
    "-loglevel", "repeat+level+info",

    // Stop ffmpeg from listening to stdin (which it does by default even if stdin isn't actually connected...).
    "-nostdin"
];

// Arguments that apply to input streams.
const realtimeInputArgs = [
    "-avioflags", "direct",
    "-fflags", "nobuffer",
    "-rtbufsize", `${pipeSize}`,
    "-thread_queue_size", "0"
];

const pipeInputArgs = [...realtimeInputArgs, "-blocksize", `${pipeSize}`];
const rtspInputArgs = [...realtimeInputArgs, "-rtsp_transport", "tcp"];

const fileInputArgs = ["-re"]; // Stream the file in realtime, rather than getting ahead.

const outputArgs = [
    // Low latency options.
    "-flush_packets", "1",
    "-fflags", "flush_packets",

    // Flag to stop ffmpeg from emitting incorrect timestamps that lead to AV desynchronization and buffer length
    // issues.
    "-copyts"
];

const dashArgs = [
    ...outputArgs,

    // Formatting options.
    "-f", "dash",

    // Emit the type of DASH manifest that allows seeking to the in-progress live-edge segment without confusion.
    "-use_timeline", "0",
    "-use_template", "1",

    // DASH segment configuration.
    "-dash_segment_type", "mp4",
    "-single_file", "0",
    "-media_seg_name", "chunk-stream$RepresentationID$-$Number%09d$.$ext$",
    "-format_options", "movflags=cmaf",
    "-frag_type", "every_frame",

    // How many segments to keep/advertise.
    "-window_size", "3", // Segments the manifest advertises.
    "-extra_window_size", "2", // Segments that are kept once evicted from the manifest for lagging clients.

    // Pedantic flags we need for standards compliance.
    "-utc_timing_url", "https://time.akamai.com/?iso",

    // Low latency options.
    "-ldash", "1",
    "-streaming", "1",
    "-index_correction", "0",

    // Upload via HTTP PUT.
    "-tcp_nodelay", "1", // I'm not sure if this does anything for HTTP/DASH.
    "-method", "PUT",
    "-remove_at_exit", "1"
];

const videoArgs = [
    ["-pix_fmt", "yuv420p"]
];

const audioArgs = [
    ["-ac", "1"]
];

// Codec-specific arguments.
const h264Args = [
    ["-tune", "zerolatency"]
];

const h265Args = h264Args;

const vp8Args = [
    ["-deadline", "realtime"],
    ["-error-resilient", "1"]
];

const vp9Args = vp8Args.concat([
    ["-tile-columns", "2"],
    ["-tile-rows", "2"],
    ["-row-mt", "1"],
    ["-frame-parallel", "1"]
]);

const av1Args = vp9Args;

// Map from video codec name to specific arguments.
const videoCodecArgs = {
    "h264": h264Args,
    "h265": h265Args,
    "vp8": vp8Args,
    "vp9": vp9Args,
    "av1": av1Args
};

// Map from audio codec name to specific arguments.
const audioCodecArgs = new Map<string, string[][]>();

/* Add external audio and video reading. */
function getExternalSourceArgs(src: string): string[] {
    let args: string[] = globalArgs;

    if (src.startsWith("pipe:")) {
        args = [...args, ...pipeInputArgs];
    } else if (src.startsWith("rtsp://")) {
        args = [...args, ...rtspInputArgs];
    } else if (src.search(/^[A-Za-z0-9]+:[/]{2}/) === 0) {
        args = [...args, ...realtimeInputArgs]; // This is a guess, but the intended use of this system is realtime.
    } else if (fs.statSync(src).isFile()) {
        args = [...args, ...fileInputArgs];
    } else if (fs.statSync(src).isFIFO()) {
        args = [...args, ...pipeInputArgs]; // This is just a pipe that is named in the filesystem.
    } else {
        args = [...args, ...realtimeInputArgs]; // This is a guess, but the intended use of this system is realtime.
    }

    args = [...args, "-i", src];

    return args;
}

/* Get a filter string to print a timestamp onto the video. */
function getTimestampFilteringArgs(enabled: boolean, width: number): string {
    if (!enabled) {
        return "";
    }
    return ",drawtext=text='%{gmtime} UTC':" +
           `x=${width / 40}:y=${width / 40}:fontsize=${width / 30}:borderw=${width / 480}:` +
           "fontfile=/usr/share/fonts/TTF/DejaVuSansMono.ttf:" +
           "fontcolor=#ffffff:bordercolor=#000000";
}

/* Add filtering. This assumes a single video input stream. The output streams are v0, v1, v2, ... */
function getFilteringArgs(videoConfigs: VideoConfig[], timestamp: boolean): string[] {
    // Split the input.
    let videoFilter = `[0:v]split=${videoConfigs.length}`;
    videoConfigs.forEach((_config: VideoConfig, i: number): void => {
        videoFilter += `[vin${i}]`;
    });
    videoFilter += "; ";

    // Filter each stream.
    videoConfigs.forEach((config: VideoConfig, i: number): void => {
        videoFilter += `[vin${i}]fps=${config.framerateNumerator}/${config.framerateDenominator},` +
                       `scale=${config.width}x${config.height}` +
                       `${getTimestampFilteringArgs(timestamp, config.width)}[v${i}]; `;
    });
    videoFilter = videoFilter.substr(0, videoFilter.length - 2);

    /* Argument time :) */
    return ["-filter_complex", videoFilter];
}

/* Get encoder arguments for a given set of codec configurations.. */
function getEncodeArgs(videoConfigs: VideoConfig[], audioConfigs: AudioConfig[], codecOptions: CodecOptions,
                       rateControlBufferLength: number): string[] {
    let args: string[] = [];

    /* Add common arguments. */
    // Video.
    videoArgs.forEach((as: string[]): void => {
        assert(as.length > 0);
        args.push(`${as[0]!}:v`);
        as.slice(1).forEach((a: string): void => {
            args.push(a);
        });
    });

    // Audio.
    audioArgs.forEach((as: string[]): void => {
        assert(as.length > 0);
        args.push(`${as[0]!}:a`);
        as.slice(1).forEach((a: string): void => {
            args.push(a);
        });
    });

    /* Stream-specific arguments. */
    // Video.
    videoConfigs.forEach((config: VideoConfig, index: number): void => {
        args = args.concat([
            // Codec.
            `-c:v:${index}`, (config.codec === "h265") ? "hevc" : config.codec,

            // Constant rate factor.
            `-crf:v:${index}`, `${config.crf}`,

            // Maximum bitrate.
            `${(config.codec === "h264" || config.codec === "h265") ? "-maxrate" : "-b"}:v:${index}`,
            `${config.bitrate}k`,

            // Rate control buffer size. Used to impose the maximum bitrate.
            `-bufsize:v:${index}`, `${config.bitrate * rateControlBufferLength / 1000}k`,

            // GOP size.
            `-g:v:${index}`, `${config.gop}`,
            `-force_key_frames:v:${index}`, `expr:eq(mod(n, ${config.gop}), 0)`
        ]);

        // Special codec-specific options.
        switch (config.codec) {
            case "h264":
                args = args.concat([`-preset:v:${index}`, `${codecOptions.h264Preset}`]);
                break;
            case "h265":
                args = args.concat([`-preset:v:${index}`, `${codecOptions.h265Preset}`]);
                break;
            case "vp8":
                args = args.concat([`-speed:v:${index}`, `${codecOptions.vp8Speed}`]);
                break;
            case "vp9":
                args = args.concat([`-speed:v:${index}`, `${codecOptions.vp9Speed}`]);
                break;
            case "av1":
                args = args.concat([`-speed:v:${index}`, `${codecOptions.av1Speed}`]);
                break;
        }

        // Codec-specific options.
        const codecArgs: string[][] = (config.codec in videoCodecArgs) ?
            videoCodecArgs[config.codec] : [];

        codecArgs.forEach((as: string[]): void => {
            assert(as.length > 0);
            args.push(`${as[0]!}:v:${index}`);
            as.slice(1).forEach((a: string): void => {
                args.push(a);
            });
        });
    });

    // Audio.
    audioConfigs.forEach((config: AudioConfig, index: number): void => {
        args = args.concat([
            // Codec.
            `-c:a:${index}`, (config.codec === "opus") ? "libopus" : config.codec,

            // Bitrate.
            `-b:a:${index}`, `${config.bitrate}k`
        ]);

        // Codec specific options.
        const codecArgs: string[][] = (config.codec in audioCodecArgs) ? audioCodecArgs.get(config.codec)! : [];

        codecArgs.forEach((as: string[]): void => {
            assert(as.length > 0);
            args.push(`${as[0]!}:a:${index}`);
            as.slice(1).forEach((a: string): void => {
                args.push(a);
            });
        });
    });

    /* Done :) */
    return args;
}

/* Get DASH output arguments. */
function getDashOutputArgs(segmentLengthMultiplier: number, videoConfigs: VideoConfig[], targetLatency: number,
                           hasAudio: boolean, port: number, manifest: string): string[] {
    /* Compute the segment duration. */
    const commonGopPeriod: [number, number] = computeSegmentDuration(segmentLengthMultiplier, videoConfigs);

    /* Specific arguments we receive in the constructor. */
    let args = [
        "-seg_duration", `${commonGopPeriod[0] / commonGopPeriod[1]}`,
        "-target_latency", `${targetLatency / 1000}`
    ];

    /* Compute the adaptation sets. */
    let adaptationSets = "id=0,streams=v";
    if (hasAudio) {
        adaptationSets += " id=1,streams=a";
    }
    args = args.concat(["-adaptation_sets", adaptationSets]);

    /* Common output arguments. */
    args = args.concat(dashArgs);

    /* The actual manifest output. */
    args.push(`http://localhost:${port}${manifest}`);

    /* Done :) */
    return args;
}

/* Get the ffmpeg arguments to give to Subprocess to transcode from external input to DASH in one go. */
export function getTranscoderArgs(angle: number, config: Config, source: string, uniqueId: string): string[] {
    const videoConfigs = config.video.configs;
    const audioConfigs = config.audio.configs;

    let args = [
        // Inputs
        ...getExternalSourceArgs(source),

        // Video filtering
        ...getFilteringArgs(videoConfigs, config.video.timestamp)
    ];

    /* Output map. */
    // Video streams from filtration.
    videoConfigs.forEach((_config: VideoConfig, i: number): void => {
        args.push("-map");
        args.push(`[v${i}]`);
    });

    // Audio stream.
    audioConfigs.forEach((): void => {
        const streamIdx = 0;

        args.push("-map");
        args.push(`${streamIdx}:a`);
    });

    args = [
        ...args,

        // Encoder arguments.
        ...getEncodeArgs(videoConfigs, audioConfigs, config.video.codecConfig,
                         config.dash.rateControlBufferLength),

        // Output arguments.
        ...getDashOutputArgs(
            config.dash.segmentLengthMultiplier,
            config.video.configs,
            config.dash.targetLatency,
            audioConfigs.length !== 0,
            config.network.port,
            substituteManifestPattern(config.dash.manifest, uniqueId, angle)
        )
    ];

    // Done :)
    return args;
}

// A parsed Ffmpeg stats line.
// interface FfmpegStats {
//     frame: number;
//     fps: number;
//     q: number;
//     time: string;
//     dup: number;
//     drop: number;
//     speed: number;
// }

export class Subprocess {
    inputStreams: stream.Writable[] = [];
    outputStreams: stream.Readable[] = [];

    private process?: child_process.ChildProcess;

    // Are we currently in the process of trying to quit?
    private stopping: boolean = false;

    private readonly log: Logger;

    public async start(): Promise<void> {
        return new Promise((resolve, reject) => {
            // Launch the process.
            this.process = child_process.spawn("ffmpeg", this.args, {stdio: [
                // Ignore stdin, capture stout and stderr.
                "ignore", "pipe", "pipe"
            ]});

            this.process.on("exit", (code: number | undefined, signal: string | undefined): void => {
                if (code !== undefined) {
                    this.log.info(`[${this.name}] Exited with status code ${code}`);
                } else {
                    this.log.info(`[${this.name}] Terminated due to signal ${signal!}`);
                }

                if (!this.stopping) {
                    process.exit(1); // FFmpeg terminating unexpectedly is an error condition.
                }
            });

            this.process.once("error", (e: Error): void => {
                reject(e);
            });

            // Print the FFmpeg command
            let argsString = "  ffmpeg";
            this.args.forEach((arg: string): void => {
                argsString += " " + ((arg.includes(" ") || this.args.includes("\"")) ?
                                     ("\"" + arg.replace(/\\/g, "\\\\").replace(/"/g, "\\\"") + "\"") : arg);
            });

            this.log.debug(`Spawning FFmpeg Command:\n${argsString}`);

            this.process.stderr!.once("data", () => {
                // This presumably means it's alive...
                resolve();
            });

            // Hand ffmpeg's output to the logging system, if it wants it.
            this.process.stdout!.on("data", (data: string | Buffer) => {
                this.printFfmpegLog(data.toString());
            });
            this.process.stderr!.on("data", (data: string | Buffer) => {
                this.handleStderr(data.toString());
            });
        });
    }

    // Map ffmpeg log-lines to the right log-level in our infrastructure.
    private printFfmpegLog(text: string): void {
        // Parse the loglevel from the start of the line. If it's not there, assume something weird is going on and
        // just emit it at level `warn`.
        if (!text.startsWith("[")) {
            this.log.warn(text);
            return;
        }

        const lines = text.split("\n");
        let currentLevel = "";
        let currentDocument = "";

        const emitDocument = (): void => {
            currentDocument = currentDocument.replace(/\s+$/, "");
            if (!currentDocument) {
                return;
            }

            switch (currentLevel) {
                case "panic":
                case "fatal":
                case "error":
                    this.log.error(currentDocument);
                    break;
                case "warning":
                    this.log.warn(currentDocument);
                    break;
                case "info":
                    this.log.info(currentDocument);
                    break;
                case "verbose":
                case "debug":
                case "trace":
                    this.log.debug(currentDocument);
                    break;
                default:
                    this.log.warn(currentDocument);
            }
        };

        for (const line of lines) {
            // Fully-blank lines just get appended.
            if (/^\s*$/.exec(line)) {
                currentDocument += "\n";
                continue;
            }

            const match = /^\s*\[([^\]]+)\](?:\s\[([^\]]+)\])?/.exec(line);

            // Sometimes there's another thing in brackets first.
            const loglevel = !match ? "warning" : (match[2] ?? match[1] ?? "warning");
            if (loglevel !== currentLevel) {
                emitDocument();

                currentLevel = loglevel;
                currentDocument = "";
            }

            // console.log("line: " + line);
            // console.log("loglevel: " + loglevel);
            const payload = line.replace(`[${loglevel}] `, "");
            currentDocument += payload + "\n";
        }

        emitDocument();
    }

    private handleStderr(line: string): void {
        // If it's one of the stats lines that come out every second, digest it. Otherwise, print it.
        if (line.slice(0, 25).includes(" frame=")) {
            // Do something with this, if you want. :D
            // const stats = Object.fromEntries(
            //     line.replaceAll(/=\s+/g, '=')
            //         .split(' ')
            //         .filter((p) => p.length > 0 && !p.endsWith('N/A'))
            //         .slice(1, -1)
            //         .map((s) => s.split('='))
            // );
            //
            // // Sort out the numeric fields.
            // stats.frame = Number(stats.frame);
            // stats.fps = Number(stats.fps);
            // stats.q = Number(stats.q);
            // stats.drop = Number(stats.drop);
            // stats.dup = Number(stats.dup);
            // stats.speed = Number(stats.speed.slice(0, -1));
            // const parsedStats = stats as FfmpegStats;
            // console.log(parsedStats);
        } else {
            this.printFfmpegLog(line);
        }
    }

    /**
     * Send SIGTERM and wait for ffmpeg to finish its finishing-off-stuff. This should lead to nicely-formatted
     * media files after we're stopped.
     *
     * @param timeoutMs After this many milliseconds, give up and send SIGKILL.
     */
    public async stop(timeoutMs: number | undefined): Promise<void> {
        this.stopping = true;

        // If we never started, or already stopped, then stopping is a no-op.
        if (!this.process || this.process.exitCode !== null) {
            return;
        }

        return new Promise((resolve): void => {
            let murderiseCountdown: Timeout | undefined = undefined;
            if (timeoutMs !== undefined) {
                murderiseCountdown = setTimeout(() => {
                    this.process!.kill("SIGKILL");
                }, timeoutMs);
            }

            let exitHandler: ((...args: any[]) => void) | null = null;
            let errorHandler: ((...args: any[]) => void) | null = null;
            exitHandler = (): void => {
                if (murderiseCountdown) {
                    clearTimeout(murderiseCountdown);
                }

                this.process!.off("error", errorHandler!);

                resolve();
            };

            errorHandler = (e: Error): void => {
                this.log.error(`[${this.name}] Error during shutdown: ${e.message}`);
                this.process!.off("exit", exitHandler!);
            };

            this.process!.once("exit", exitHandler);
            this.process!.on("error", errorHandler);

            this.process!.kill("SIGTERM");
        });
    }

    constructor(private readonly name: string,
                private readonly args: string[]) {
        this.log = new Logger("Ffmpeg " + name);
    }
}
