import {assertNonNull} from "../common/util/Assertion";

export type AudioCodec = 'aac' | 'opus';
export type VideoCodec = 'h264' | 'h265' | 'vp8' | 'vp9' | 'av1';

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

    // Frame rate for the next video stream, as a rational.
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

        // Manifest path pattern. {N} is replaced by the angle number, with N digits. {unix} is replaced by a unix
        // timestamp at the time the server starts up.
        manifest: string,

        // Segment duration in seconds.
        segmentDuration: number,

        // Advertized target latency in milliseconds.
        targetLatency: number
    },
    misc: {
        // Use a single-process ffmpeg pipeline for each angle. If false, a multi-process pipeline is used.
        monolithic: boolean
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
        sources: string[]
    }
}

export function substituteManifestPattern(pattern: string, index?: number): string {
    let result = pattern.replaceAll('{unix}', '' + Math.round(Date.now() / 1000));

    // eslint-disable-next-line no-constant-condition
    while (true) {
        // eslint-disable-next-line @typescript-eslint/prefer-regexp-exec
        const match = result.match('[{]([0-9]+)[}]');
        if (match == null) {
            break;
        }

        assertNonNull(match);
        assertNonNull(match.index);

        const startIdx = match.index;
        const endIdx = match.index + match[0].length;
        const numDigits = parseInt(match[1]);

        const replacement = (index === undefined) ?
            ('[0-9]{' + numDigits + ',}') : index.toString().padStart(numDigits, '0');
        result = result.substr(0, startIdx) + replacement + result.substr(endIdx);
    }
    return result;
}
