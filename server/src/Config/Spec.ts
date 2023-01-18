/**
 * Audio codec.
 */
export type AudioCodec = "aac" | "opus";

/**
 * Video codec.
 */
export type VideoCodec = "h264" | "h265" | "vp8" | "vp9" | "av1";

/**
 * Preset for h264 and h265 encoding complexity.
 *
 * There is a trade-off between bitrate, quality, and encoding speed. Values of this type adjust the trade-off exchange
 * bitrate and quality for faster encoding. This must be set so that encoding is reliably realtime. See the
 * [preset section of the FFmpeg h264 encoding guide](https://trac.ffmpeg.org/wiki/Encode/H.264#Preset) for more
 * information.
 */
export type H264Preset = "ultrafast" | "superfast" | "veryfast" | "faster" | "fast" | "medium" | "slow" | "slower" |
                         "veryslow" | "placebo";

/**
 * Information to control client-side buffering.
 *
 * This is a per video-configuration object because different video configurations (especially bitrate) might require
 * different buffering parameters.
 */
export interface BufferControl {
    /**
     * Extra buffer above the maximum observed when seeking.
     *
     * The default is set to be 3 Opus frames (180 ms) to stop audio buffering causing seeking events. Increasing this
     * parameter makes playback smoother at the expense of latency.
     */
    extraBuffer: number,

    /**
     * The size of the target buffer when a stream is first played, in ms.
     *
     * If this buffer is exceeded before a buffer history is built, it's considered that the player has fallen behind.
     */
    initialBuffer: number,

    /**
     * The buffer to keep when seeking to the live edge, in ms.
     *
     * Increasing this can prevent additional stalls after a seek and after starting to play a stream.
     */
    seekBuffer: number,

    /**
     * The minimum time to wait before doing the initial seek to get the stream playing.
     *
     * Increasing this reduces stuttering at the start at the expense of delaying the video playback. It should not
     * adversely affect latency once playback has begun.
     */
    minimumInitTime: number
}

/**
 * Audio configuration for a single stream quality.
 */
export interface AudioConfig {
    // The bitrate in kBit/s for this audio stream.
    bitrate: number,

    // The codec for this audio stream.
    codec: AudioCodec
}

/**
 * Video configuration for a single stream quality.
 */
export interface VideoConfig {
    /**
     * The bitrate limit in kBit/s for this video stream.
     */
    bitrate: number,

    /**
     * The minimum bitrate in kBit/s for this video stream.
     *
     * This is useful for force-purging CDN buffers. There are trade-offs between bandwidth required, and latency in the
     * presence of buffers. The default attempts to choose an appropriate value, but you may want to do the trade-off
     * differently. This feature can be disabled entirely by setting to 0.
     *
     * Not all codecs support this. Currently, only h264 and h265 do.
     */
    minBitrate: number | null,

    /**
     * The minimum bitrate in kBit/s for any interleave that contains this video stream.
     *
     * If the specified bitrate is not achieved naturally, then the interleave is padded with extra data to achieve the
     * minimum rate.
     */
    minInterleaveRate: number;

    /**
     * The window, in ms, over which the minimum interleave rate is evaluated.
     *
     * The rate is evaluated twice per window, so the worst case scenario is no data transmitted for 1.5x the window.
     */
    minInterleaveWindow: number;

    /**
     * Constant rate factor.
     *
     * For h264, see the [CRF section of the Ffmpeg h264 encoding guide](https://trac.ffmpeg.org/wiki/Encode/H.264#crf).
     * Other codecs have different CRF characteristics.
     */
    crf: number,

    /**
     * The codec for this video stream.
     */
    codec: VideoCodec,

    /**
     * Frame rate numerator for the video stream.
     *
     * For the smoothest playback, it's recommended that this is chosen so that the camera's frame rate is an integer
     * multiple of this. If not, then frames will be dropped or inserted at non-constant intervals to achieve the
     * requested frame rate.
     */
    framerateNumerator: number,

    /**
     * Frame rate denominator.
     */
    framerateDenominator: number,

    /**
     * Group of pictures size in frames.
     *
     * The segment duration is computed so that it is a minimal integer multiple of GOPs for each stream, multiplied by
     * the dash.segmentLengthMultiplier configuration option. Because of this lowest common multiple behaviour, care
     * must be taken to set the GOPs so that the segment duration is reasonable. It is recommended that the GOP be
     * chosen so that gop * framerateDenominator * 1000000 / framerateNumerator can be represented exactly as an
     * integer, as DASH represents durations in integer microseconds. Segments that are not a multiple of the GOP size
     * end up varying in size by the GOP size, and this causes problems for clients' calculation of the segment index.
     */
    gop: number,

    /**
     * The width for this video stream
     */
    width: number,

    /**
     * The height for this video stream
     */
    height: number

    /**
     * Client-side buffer control settings.
     *
     * This is per video-configuration because the video configuration (and other video foibles), especially the
     * bitrate, can have a big effect on the needed buffering parameters.
     */
    bufferCtrl: BufferControl
}

/**
 * Source configuration.
 *
 * This is useful where ffmpeg needs more information than a source string can provide, such as for piped input, or
 * certain Decklink conditions.
 */
export interface SourceConfig {
    /**
     * The source (given after -i to ffmpeg).
     */
    source: string,

    /**
     * Extra input arguments to give for the source.
     */
    extraInputArgs: string[]
}

/**
 * Tuning parameters for codecs.
 */
export interface CodecOptions {
    /**
     * The speed ([0-15]) for av1.
     */
    av1Speed: number,

    /**
     * The speed ([0-15]) for vp8.
     */
    vp8Speed: number,

    /**
     * The speed ([0-15]) for vp9.
     */
    vp9Speed: number,

    /**
     * The preset for h264.
     */
    h264Preset: H264Preset,

    /**
     * The preset for h265 (same set of options as for h264).
     */
    h265Preset: H264Preset
}

/**
 * Configuration to serve a directory from the filesystem.
 */
export interface FilesystemDirectoryConfig {
    /**
     * The URL prefix to match.
     */
    urlPrefix: string,

    /**
     * The path in the filesystem to serve from.
     */
    path: string,

    /**
     * The filename to use if none is given in the request.
     */
    index: string,

    /**
     * Whether or not this directory should be accessible only in secure contexts (e.g: localhost.)
     */
    secure: boolean,

    /**
     * Whether or not the cache control should be ephemeral.
     */
    ephemeral: boolean
}

/**
 * Server configuration.
 */
export interface Config {
    /**
     * Audio configuration.
     */
    audio: {
        /**
         * For each source, generate this set of output audio streams.
         */
        configs: AudioConfig[]
    },

    /**
     * DASH configuration.
     *
     * This contains configuration that would be applicable to DASH.
     */
    dash: {
        /**
         * Multiply the segment length inferred from the GOP size by this amount.
         *
         * This should be an integer to avoid segment length variations. It is recommended that the segments be
         * approximately 8-16 seconds in duration. It can be found in the ffmpeg command as the `-seg_duration` value.
         */
        segmentLengthMultiplier: number,

        /**
         * Rate control buffer length in milliseconds.
         */
        rateControlBufferLength: number,

        /**
         * Manifest path pattern.
         *
         * {N} is replaced by the angle number, with N digits. {uid} is replaced by a string that
         * is unique to every server startup.
         */
        manifest: string,

        /**
         * Advertized target latency in milliseconds.
         */
        targetLatency: number,

        /**
         * The limit, in ms, of how far segment start times are allowed to drift before the server terminates itself.
         * Set to zero to disable.
         */
        terminateDriftLimit: number,

        /**
         * Whether or not to interleave the audio and video streams. This helps evict audio data from the CDN buffer.
         */
        interleave: boolean,

        /**
         * Whether or not to allow direct access to dash segments that are interleaved.
         *
         * This should be false for the production server so that it doesn't have to upload video twice. This also helps
         * the client switch audio and video at the same time, without accidentally downloading mismatched audio and
         * video segments (and therefore two video streams). It is useful to be able to set this to true for development
         * and testing.
         */
        interleavedDirectDashSegments: boolean,

        /**
         * Average period between timestamps that are interleaved into the same interleave.
         *
         * The timestamps are used for buffer control.
         */
        interleaveTimestampInterval: number
    },

    /**
     * Configuration for serving from the filesystem.
     */
    filesystem: {
        /**
         * A list of directory configurations to serve.
         */
        directories: FilesystemDirectoryConfig[],

        /**
         * A map of mimetypes. Use null to remove a builtin mimetype.
         */
        mimetypes: Record<string, string | null>
    },

    /**
     * Networking and HTTP configuration.
     */
    network: {
        /**
         * Time to cache responses to non-live paths.
         */
        nonLiveCacheTime: number,

        /**
         * The value to use for the Access-Control-Allow-Origin HTTP header.
         *
         * If not given, this header will not be sent.
         */
        origin?: string,

        /**
         * Port number to listen on.
         */
        port: number,

        /**
         * Time before the start of the next segment to start accepting requests for it.
         *
         * Due to HTTP cache timing resolution limitations, this should be greater than 1000.
         */
        preAvailabilityTime: number,

        /**
         * The number of old segments to cache.
         */
        segmentRetentionCount: number
    },

    /**
     * Server information API configuration.
     */
    serverInfo: {
        /**
         * Output live-stream server information (e.g: available angles) to this path.
         */
        live: string
    },

    /**
     * Video configuration.
     */
    video: {
        /**
         * Server-wide codec options.
         */
        codecConfig: CodecOptions,

        /**
         * For each source, generate this set of output video streams.
         */
        configs: VideoConfig[],

        /**
         * Whether or not to loop sources indefinitely if possible.
         */
        loop: boolean,

        /**
         * List of video angle sources.
         *
         * For Decklink, prepend the name ffmpeg uses with "decklink://". This will make the server add "-f decklink" as
         * necessary.
         */
        sources: (string|SourceConfig)[],

        /**
         * Whether or not to print a timestamp onto the video.
         */
        timestamp: boolean
    }
}
