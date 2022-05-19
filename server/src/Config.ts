import {readFileSync} from "fs";
import * as is from "typescript-is";
import * as ckis from "@ckitching/typescript-is";
import {assertNonNull} from "live-video-streamer-common";

export type AudioCodec = "aac" | "opus";
export type VideoCodec = "h264" | "h265" | "vp8" | "vp9" | "av1";

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
    h264Preset: string,

    // The preset for h265.
    h265Preset: string,
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

export function loadConfig(path: string): Config {
    const json: string = readFileSync(path, {encoding: "utf-8"});
    const obj: unknown = JSON.parse(json);
    if (!ckis.equals<Config>(obj)) {
        try {
            is.assertEquals<Config>(obj);
            throw new Error("Invalid configuration.");
        } catch (e) {
            throw new Error(`Invalid configuration: ${(e as Error).message}`);
        }
    }
    return obj;
}
