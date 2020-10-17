import * as child_process from 'child_process';
import * as process from 'process';
import * as stream from 'stream';
import {Logger} from "winston";
import {getLogger} from "../common/LoggerConfig";
import {AudioConfig, CodecOptions, Config, VideoConfig} from "./Config";

export const log: Logger = getLogger("Ffmpeg");

/* FFMPEG Arguments. */
const pipeSize = 1024; // Buffer size for pipes that ffmpeg uses to communicate.
const minimalDuration = 10000; // A minimal duration for muxing parameters in microseconds.

// Common arguments/
const inputArgs = [
    '-avioflags', 'direct',
    '-fflags', 'nobuffer',
    '-rtbufsize', '' + pipeSize,
    '-thread_queue_size', '0'
];

const pipeInputArgs = inputArgs.concat(['-blocksize', '' + pipeSize]);
const rtspInputArgs = inputArgs.concat(['-rtsp_transport', 'tcp']);

const outputArgs = [
    // Low latency options.
    '-flush_packets', '1',
    '-fflags', 'flush_packets',
    '-chunk_duration', '10000',

    // Try and tweak the AV interleaving to reduce latency.
    '-max_interleave_delta', '' + minimalDuration,
    '-max_delay', '' + minimalDuration
];

const dashArgs = [
    ...outputArgs,

    // Formatting options.
    '-f', 'dash',

    // Emit the type of DASH manifest that allows seeking to the in-progress live-edge segment without confusion.
    '-use_timeline', '0',
    '-use_template', '1',

    // DASH segment configuration.
    '-dash_segment_type', 'mp4',
    '-single_file', '0',
    '-format_options', 'movflags=cmaf',
    '-frag_type', 'duration',
    '-frag_duration', '' + (minimalDuration / 1000000),

    // How many segments to keep/advertise.
    '-window_size', '3', // Segments the manifest advertises.
    '-extra_window_size', '2', // Segments that are kept once evicted from the manifest for lagging clients.

    // Try and tweak the AV interleaving to reduce latency.
    '-audio_preload', '' + minimalDuration,

    // Pedantic flags we need for standards compliance.
    '-utc_timing_url', 'https://time.akamai.com/?iso',

    // Low latency options.
    '-ldash', '1',
    '-streaming', '1',
    '-index_correction', '1',

    // Upload via HTTP PUT.
    `-tcp_nodelay`, '1', // I'm not sure if this does anything for HTTP/DASH.
    '-method', 'PUT',
    '-remove_at_exit', '1'
];

const videoArgs = [
    ['-pix_fmt', 'yuv420p']
];

const audioArgs = [
    ['-ac', '1']
];

// Codec-specific arguments.
const h264Args = [
    ['-tune', 'zerolatency']
];

const h265Args = h264Args;

const vp8Args = [
    ['-deadline', 'realtime'],
    ['-error-resilient', '1']
];

const vp9Args = vp8Args.concat([
    ['-tile-columns', '2'],
    ['-tile-rows', '2'],
    ['-row-mt', '1'],
    ['-frame-parallel', '1']
]);

const av1Args = vp9Args;

// Map from codec name to specific arguments.
const videoCodecArgs = {
    'h264': h264Args,
    'h265': h265Args,
    'vp8': vp8Args,
    'vp9': vp9Args,
    'av1': av1Args
};

// TODO: WTF is this, Nick?
const audioCodecArgs: any = {};

/* Add external audio and video reading. */
function getExternalSourceArgs(src: string): string[] {
    let args: string[];

    if (src.startsWith('pipe:')) {
        args = pipeInputArgs;
    } else if (src.startsWith('rtsp://')) {
        args = rtspInputArgs;
    } else {
        args = inputArgs;
    }

    args = args.concat(['-i', src]);

    return args;
}

/* Add filtering. This assumes a single video input stream. The output streams are v0, v1, v2, ... */
function getFilteringArgs(videoConfigs: VideoConfig[]): string[] {
    // Split the input.
    let videoFilter = "[0:v]split=" + videoConfigs.length;
    videoConfigs.forEach((_config: VideoConfig, i: number): void => {
        videoFilter += `[vin${i}]`;
    });
    videoFilter += '; ';

    // Filter each stream.
    videoConfigs.forEach((config: VideoConfig, i: number): void => {
        videoFilter += `[vin${i}]fps=${config.framerateNumerator}/${config.framerateDenominator},scale=${config.width}x${config.height}[v${i}]; `;
    });
    videoFilter = videoFilter.substr(0, videoFilter.length - 2);

    /* Argument time :) */
    return ['-filter_complex', videoFilter];
}

/* Get encoder arguments for a given set of codec configurations.. */
function getEncodeArgs(videoConfigs: VideoConfig[], audioConfigs: AudioConfig[], codecOptions: CodecOptions,
                       fragmentDuration: number): string[] {
    let args: string[] = [];

    /* Add common arguments. */
    // Video.
    videoArgs.forEach((as: string[]): void => {
        args.push(as[0] + ':v');
        as.slice(1).forEach((a: string): void => {
            args.push(a);
        });
    });

    // Audio.
    audioArgs.forEach((as: string[]): void => {
        args.push(as[0] + ':a');
        as.slice(1).forEach((a: string): void => {
            args.push(a);
        });
    });

    /* Stream-specific arguments. */
    // Video.
    videoConfigs.forEach((config: VideoConfig, index: number): void => {
        args = args.concat([
            // Codec.
            '-c:v:' + index, config.codec,

            // Bitrate.
            '-b:v:' + index, config.bitrate + 'k',

            // GOP size.
            '-g:v:' + index, '' + Math.round((fragmentDuration * config.framerateNumerator) /
                (config.framerateDenominator * 1000))
        ]);

        // Special codec-specific options.
        switch (config.codec) {
            case 'h264':
                args = args.concat(['-preset:v:' + index, codecOptions.h264Preset]);
                break;
            case 'h265':
                args = args.concat(['-preset:v:' + index, codecOptions.h265Preset]);
                break;
            case 'vp8':
                args = args.concat(['-speed:v:' + index, '' + codecOptions.vp8Speed]);
                break;
            case 'vp9':
                args = args.concat(['-speed:v:' + index, '' + codecOptions.vp9Speed]);
                break;
            case 'av1':
                args = args.concat(['-speed:v:' + index, '' + codecOptions.av1Speed]);
                break;
        }

        // Codec-specific options.
        const codecArgs: string[][] = (config.codec in videoCodecArgs) ?
            videoCodecArgs[config.codec] : [];

        codecArgs.forEach((as: string[]): void => {
            args.push(as[0] + ':v:' + index);
            as.slice(1).forEach((a: string): void => {
                args.push(a);
            });
        });
    });

    // Audio.
    audioConfigs.forEach((config: AudioConfig, index: number): void => {
        args = args.concat([
            // Codec.
            '-c:a:' + index, config.codec,

            // Bitrate.
            '-b:a:' + index, config.bitrate + 'k'
        ]);

        // Codec specific options.
        const codecArgs: string[][] = (config.codec in audioCodecArgs) ? audioCodecArgs[config.codec] : [];

        codecArgs.forEach((as: string[]): void => {
            args.push(as[0] + ':a:' + index);
            as.slice(1).forEach((a: string): void => {
                args.push(a);
            });
        });
    });

    /* Done :) */
    return args;
}

/* Get DASH output arguments. */
function getDashOutputArgs(segmentDuration: number, targetLatency: number, hasAudio: boolean,
                           port: number, manifest: string): string[] {
    /* Specific arguments we receive in the constructor. */
    let args = [
        '-seg_duration', '' + segmentDuration,
        '-target_latency', '' + (targetLatency / 1000)
    ];

    /* Compute the adaptation sets. */
    let adaptationSets = 'id=0,streams=v';
    if (hasAudio) {
        adaptationSets += ' id=1,streams=a';
    }
    args = args.concat(['-adaptation_sets', adaptationSets]);

    /* Common output arguments. */
    args = args.concat(dashArgs);

    /* The actual manifest output. */
    args.push('http://localhost:' + port + manifest);

    /* Done :) */
    return args;
}

export namespace ffmpeg {
    export class Subprocess {
        inputStreams: stream.Writable[] = [];
        outputStreams: stream.Readable[] = [];

        constructor(verbose: boolean, ffmpegOpType: string, args: string[]) {
            // construct the stdio argument.
            let stdio = new Array<any>();

            // We never want to connect stdin.
            stdio.push('ignore');

            // Stdout and stderr should be redirected to null if we're not going to collect the data.
            stdio = stdio.concat(verbose ? ['pipe', 'pipe'] : ['ignore', 'ignore']);

            /* Launch the process. */
            const subprocess = child_process.spawn('ffmpeg', args, {stdio: stdio});
            subprocess.on('exit', (): void => {
                console.log(ffmpegOpType + ' FFmpeg process terminated');
                process.exit(1); // FFmpeg terminating is an error condition.
            });

            // Print the FFmpeg command
            let argsString = '  ffmpeg';
            args.forEach((arg: string): void => {
                argsString += ' ' + ((arg.includes(' ') || args.includes('"')) ?
                    ('"' + arg.replace(/\\/g, '\\\\').replace(/"/g, '\\"') + '"') : arg);
            });
            log.debug(`Started FFmpeg Command:\n${argsString}`);

            /* Be verbose. */
            if (!verbose) {
                return;
            }

            // Redirect FFmpeg's console output.
            subprocess.stdout.pipe(process.stdout);
            subprocess.stderr.pipe(process.stderr);
        }
    }

    /* Run ffmpeg to transcode from external input to DASH in one go. */
    export function launchTranscoder(verbose: boolean, config: Config, source: string): Subprocess {
        const videoConfigs = config.video.configs;
        const audioConfigs = config.audio.configs;

        // TODO: Suspect?
        const fragmentDuration = config.dash.gop;

        let args = [
            // Inputs
            ...getExternalSourceArgs(source),

            // Video filtering
            ...getFilteringArgs(videoConfigs),
        ];

        /* Output map. */
        // Video streams from filtration.
        videoConfigs.forEach((_config: VideoConfig, i: number): void => {
            args.push('-map');
            args.push(`[v${i}]`);
        });

        // Audio stream.
        audioConfigs.forEach((): void => {
            const streamIdx = 0;

            args.push('-map');
            args.push(streamIdx + ':a');
        });

        args = [
            ...args,

            // Encoder arguments.
            ...getEncodeArgs(videoConfigs, audioConfigs, config.video.codecConfig, fragmentDuration),

            // Output arguments.
            ...getDashOutputArgs(
                config.dash.segmentDuration,
                config.dash.targetLatency,
                audioConfigs.length != 0,
                config.network.port,
                config.dash.manifest
            )
        ];

        // Launch the subprocess.
        return new Subprocess(verbose, 'Transcode', args);
    }
}
