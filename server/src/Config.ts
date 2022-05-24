import {readFileSync} from "fs";
import * as is from "typescript-is";
import * as ckis from "@ckitching/typescript-is";
import {assertNonNull} from "live-video-streamer-common";

export type AudioCodec = "aac" | "opus";
export type VideoCodec = "h264" | "h265" | "vp8" | "vp9" | "av1";
export type H264Preset = "ultrafast" | "superfast" | "veryfast" | "faster" | "fast" | "medium" | "slow" | "slower" |
                         "veryslow" | "placebo";

export interface AudioConfig {
    // The bitrate in kBit/s for this audio stream.
    bitrate: number,

    // The codec for this audio stream.
    codec: AudioCodec
}

export interface VideoConfig {
    // The bitrate limit in kBit/s for this video stream.
    bitrate: number,

    // Constant rate factor. For h264, see this guide: https://trac.ffmpeg.org/wiki/Encode/H.264#crf. Other codecs have
    // different CRF characteristics.
    crf: number,

    // The codec for this video stream.
    codec: VideoCodec

    // Frame rate for the next video stream, as a rational. For the smoothest playback, it's recommended that this is
    // chosen so that the camera's frame rate is an integer multiple of this. If not, then frames will be dropped or
    // inserted at non-constant intervals to achieve the requested frame rate.
    framerateNumerator: number,
    framerateDenominator: number,

    // Group of pictures size in frames. The segment duration is computed so that it is a minimal integer multiple of
    // GOPs for each stream, multiplied by the dash.segmentLengthMultiplier configuration option. Because of this lowest
    // common multiple behaviour, care must be taken to set the GOPs so that the segment duration is reasonable. It is
    // recommended that the GOP be chosen so that gop * framerateDenominator * 1000000 / framerateNumerator can be
    // represented exactly as an integer, as DASH represents durations in integer microseconds. Segments that are not a
    // multiple of the GOP size end up varying in size by the GOP size, and this causes problems for clients'
    // calculation of the segment index.
    gop: number,

    // The resolution for this next video stream
    width: number;
    height: number;
}

export interface CodecOptions {
    // The speed ([0-15]) for av1.
    av1Speed: number,

    // The speed ([0-15]) for vp8.
    vp8Speed: number,

    // The speed ([0-15]) for vp9.
    vp9Speed: number,

    // The preset for h264.
    h264Preset: H264Preset,

    // The preset for h265 (same set of options as for h264).
    h265Preset: H264Preset
}

export interface Config {
    audio: {
        // For each source, generate this set of output audio streams.
        configs: AudioConfig[]
    },
    dash: {
        // Multiply the segment length inferred from the GOP size by this amount. This should be an integer to avoid
        // segment length variations. It is recommended that the segments be approximately 8-16 seconds in duration. It
        // can be found in the ffmpeg command as the `-seg_duration` value.
        segmentLengthMultiplier: number,

        // Rate control buffer length in milliseconds.
        rateControlBufferLength: number,

        // Manifest path pattern. {N} is replaced by the angle number, with N digits. {uid} is replaced by a string that
        // is unique to every server startup.
        manifest: string,

        // Advertized target latency in milliseconds.
        targetLatency: number,

        // The limit, in ms, of how far segment start times are allowed to drift before the server terminates itself.
        // Set to zero to disable.
        terminateDriftLimit: number

        // Whether or not to interleave the audio and video streams. This helps evict audio data from the CDN buffer.
        interleave: boolean

        // Whether or not to allow direct access to dash segments that are interleaved. This should be false for the
        // production server so that it doesn't have to upload video twice. This also helps the client switch audio and
        // video at the same time, without accidentally downloading mismatched audio and video segments (and therefore
        // two video streams). It is useful to be able to set this to true for development and testing.
        interleavedDirectDashSegments: boolean;
    },
    network: {
        // Time to cache responses to non-live paths.
        nonLiveCacheTime: number,

        // The value to use for the Access-Control-Allow-Origin HTTP header. If not given, this
        // header will not be sent.
        origin?: string,

        // Port number to listen on.
        port: number,

        // Time before the start of the next segment to start accepting requests for it. Due to HTTP
        // cache timing resolution limitations, this should be greater than 1000.
        preAvailabilityTime: number,

        // The number of old segments to cache.
        segmentRetentionCount: number
    },
    serverInfo: {
        // Output live-stream server information (e.g: available angles) to this path.
        live: string
    },
    video: {
        // Server-wide codec options.
        codecConfig: CodecOptions,

        // For each source, generate this set of output video streams.
        configs: VideoConfig[],

        // List of video angle sources (as RTSP streams).
        sources: string[],

        // Whether or not to print a timestamp onto the video.
        timestamp: boolean
    }
}

const defaultVideoConfig = {
    bitrate: 3000,
    crf: 25,
    width: 1920,
    height: 1080,
    framerateNumerator: 30000,
    framerateDenominator: 1001,
    gop: 450,
    codec: "h264"
};

const defaultAudioConfig = {
    bitrate: 48,
    codec: "aac"
};

const defaultConfig = {
    audio: {
        configs: []
    },
    dash: {
        segmentLengthMultiplier: 1,
        rateControlBufferLength: 1000,
        manifest: "/live/{uid}/angle-{1}/manifest.mpd",
        targetLatency: 500,
        terminateDriftLimit: 0,
        interleave: true,
        interleavedDirectDashSegments: false
    },
    network: {
        nonLiveCacheTime: 600,
        origin: "*",
        port: 8080,
        preAvailabilityTime: 4000,
        segmentRetentionCount: 6
    },
    serverInfo: {
        live: "/live/info.json"
    },
    video: {
        codecConfig: {
            av1Speed: 8,
            vp8Speed: 8,
            vp9Speed: 8,
            h264Preset: "faster",
            h265Preset: "faster"
        },
        configs: [defaultVideoConfig],
        sources: [],
        timestamp: false
    }
};

export function computeSegmentDuration(segmentLengthMultiplier: number, videoConfigs: VideoConfig[]): [number, number] {
    // Some utility functions.
    const gcd = (a: number, b: number): number => {
        if (b === 0) {
            return a;
        }
        return gcd(b, a % b); // Yay for Euclid's algorithm.
    };
    const lcm = (a: number, b: number): number => {
        return (a * b) / gcd(a, b);
    };

    // Figure out a common time-base. The fraction we're computing is a duration. But we're computing it based on frame
    // rates, which are a reciprocal time period.
    let commonGopPeriodDenominator = 1;
    for (const config of videoConfigs) {
        commonGopPeriodDenominator = lcm(commonGopPeriodDenominator, config.framerateNumerator);
    }

    // Figure out the time, in the common time-base, that is the smallest integer multiple of every GOP.
    let commonGopPeriodNumerator = 1;
    for (const config of videoConfigs) {
        const configGopPeriodNumerator =
            config.framerateDenominator * commonGopPeriodDenominator / config.framerateNumerator;
        commonGopPeriodNumerator = lcm(configGopPeriodNumerator * config.gop, commonGopPeriodNumerator);
    }

    // Multiply that by the segment length multiplier.
    commonGopPeriodNumerator *= segmentLengthMultiplier;
    const simplify = gcd(commonGopPeriodNumerator, commonGopPeriodDenominator);
    commonGopPeriodNumerator /= simplify;
    commonGopPeriodDenominator /= simplify;

    // Done :)
    return [commonGopPeriodNumerator, commonGopPeriodDenominator];
}

export function substituteManifestPattern(pattern: string, uniqueId: string, index?: number): string {
    let result = pattern.replace(/{uid}/g, uniqueId);
    while (true) {
        const match = /[{]([0-9]+)[}]/.exec(result);
        if (match === null) {
            break;
        }

        assertNonNull(match);
        assertNonNull(match.index);

        const startIdx = match.index;
        const endIdx = match.index + match[0]!.length;
        const numDigits = parseInt(match[1]!);

        const replacement = (index === undefined) ?
                            (`[0-9]{${numDigits},}`) : index.toString().padStart(numDigits, "0");
        result = result.substr(0, startIdx) + replacement + result.substr(endIdx);
    }
    return result;
}

/* Apply an Object.assign(...dst, ...src) like operation, but recurse into objects. */
function apply(dst: unknown, src: unknown): unknown {
    if (src === undefined) {
        return dst;
    }

    if (!(src instanceof Object) || !(dst instanceof Object) || dst instanceof Array) {
        return src;
    }

    const keys = Object.keys(dst);
    if (keys.length === 0) {
        return src;
    }

    const result: unknown = {};
    for (const key of keys) {
        Object.defineProperty(result, key, {
            value: apply(Object.getOwnPropertyDescriptor(dst, key)!.value,
                         Object.getOwnPropertyDescriptor(src, key)?.value),
            enumerable: true,
            writable: true
        });
    }
    for (const key of Object.keys(src)) {
        if (keys.includes(key)) {
            continue;
        }
        Object.defineProperty(result, key, {
            value: Object.getOwnPropertyDescriptor(src, key)!.value as unknown,
            enumerable: true,
            writable: true
        });
    }
    return result;
}

export function applyDefaultConfig(config: unknown): Config {
    /* Apply defaults for the configuration. */
    // The following apply() imposes a superset of the following keys (needed later) on the result.
    interface ConfigPart {
        video: {
            configs: unknown[]
        },
        audio: {
            configs: unknown[]
        }
    }

    // Most of the configuration.
    const result = apply(defaultConfig, config) as ConfigPart;

    // Video configuration.
    for (let i = 0; i < result.video.configs.length; i++) {
        result.video.configs[i] = apply(defaultVideoConfig, result.video.configs[i]);
    }

    // Audio configuration.
    for (let i = 0; i < result.audio.configs.length; i++) {
        result.audio.configs[i] = apply(defaultAudioConfig, result.audio.configs[i]);
    }

    if (!ckis.equals<Config>(result)) {
        try {
            is.assertEquals<Config>(result);
            throw new Error("Invalid configuration.");
        } catch (e) {
            throw new Error(`Invalid configuration: ${(e as Error).message}`);
        }
    }
    return result;
}

export function loadConfig(path: string): Config {
    const json: string = readFileSync(path, {encoding: "utf-8"});
    return applyDefaultConfig(JSON.parse(json));
}
