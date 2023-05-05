/**
 * API shared between the server and the client.
 */
export namespace API {
    /**
     * Buffer control information.
     */
    export interface BufferControl {
        minBuffer: number // The minimum client-side buffer to use.
        extraBuffer: number // Extra buffer, on top of the theoretical minimum measured, in ms.
        initialBuffer: number // The initial target buffer, before any history has been built, in ms.
        seekBuffer: number // The buffer to keep when seeking to the live edge, in ms.
        minimumInitTime: number // Time, in ms, to wait before doing the initial seek.
    }

    /**
     * Stream index info objects we receive.
     */
    export interface SegmentIndexDescriptor {
        index: number;
        indexWidth: number;
        age: number;
    }

    /**
     * ServerInfo video configuration.
     */
    export interface VideoConfig {
        codec: string,
        bitrate: number,
        width: number,
        height: number,
        bufferCtrl: BufferControl
    }

    /**
     * ServerInfo audio configuration.
     */
    export interface AudioConfig {
        codec: string,
        bitrate: number
    }

    /**
     * Information about a single channel.
     *
     * This is exposed as `/live/live.json` by default.
     */
    export interface ServerInfo {
        path: string,
        segmentDuration: number,
        segmentPreavailability: number,
        videoConfigs: VideoConfig[],
        audioConfigs: AudioConfig[],
        avMap: [number, number | null][]
    }

    /**
     * The type of data in a control chunk.
     */
    export enum ControlChunkType {
        jsonObject = 32,
        userJsonObject = 48,
        userBinaryData = 49,
        userString = 50,
        discard = 255
    }

    /**
     * Index of channels available on the server.
     *
     * Contains a mapping of channel URLs (see `ServerInfo` interface) to channel names.
     */
    export interface ChannelIndex {
        [infoUrl: string]: string | null,
    }

    /**
     * Content for the jsonObject control chunk type.
     */
    export interface JsonObjectControlChunk {
        /**
         * The object type.
         */
        type: string;

        /**
         * The object itself.
         */
        content: any;
    }

    /**
     * Sent to tell the client to start playing interjections.
     *
     * Timestamps and times are measured in ms. Presentation timestamps refer to those of the main stream. The PTSs of
     * the main stream are soft-assumed to increase in real time, but seek events can cause this assumption to be
     * violated.
     *
     * It is not valid to receive two requests that could overlap (including by maxSelectTime,
     * playingInterjectionPriorityTime, and so on).
     */
    export interface InterjectionRequest {
        /**
         * The URL of the interjection set to play interjections from.
         */
        setUrl: string;

        /**
         * Maximum amount of time before playback that the selection process is allowed to start.
         */
        maxSelectTime: number;

        /**
         * Minimum amount of time before playback that the selection process is allowed to start.
         */
        minSelectTime: number;

        /**
         * Earliest presentation timestamp that the playback is allowed to start.
         */
        minPlayStartPts: number;

        /**
         * Latest presentation timestamp that the playback is allowed to start.
         */
        maxPlayStartPts: number;

        /**
         * Earliest presentation timestamp that the playback is allowed to end.
         */
        minPlayEndPts: number;

        /**
         * Latest presentation timestamp that the playback is allowed to end.
         */
        maxPlayEndPts: number;

        /**
         * Whether to continue to play the main stream as a fallback to showing any interjections.
         *
         * If true, the main stream is played if the first interjection cannot be played by minPlayStartPts. Otherwise,
         * nothing plays until minPlayEndPts.
         *
         * This governs what happens if the planning algorithm can't meet the following criteria for the first
         * interjection:
         *  - The interjection selection cannot start before minSelectTime.
         *  - It cannot start playing between minPlayStartPts and maxPlayStartPts (including because the minimum buffer
         *    requirements weren't met in time), and finish playing between minPlayEndPts and maxPlayEndPts.
         */
        mainStreamFallbackInitial: boolean;

        /**
         * Whether to continue to play the main stream as a fallback to showing any interjections.
         *
         * If true, the main stream is played if interjections cannot be played until minPlayStartPts. Otherwise,
         * nothing plays until minPlayStartPts.
         *
         * This governs what happens if the planning algorithm can't meet the following criteria for an interjection
         * that is not the first:
         *  - The interjection selection cannot start before minSelectTime.
         *  - It cannot start playing between minPlayStartPts and maxPlayStartPts (including because the minimum buffer
         *    requirements weren't met in time), and finish playing between minPlayEndPts and maxPlayEndPts.
         */
        mainStreamFallbackSubsequent: boolean;

        /**
         * The amount of time past maxPlayEndPts that an interjection that is already playing has priority over the main
         * stream if it stalls long enough to fail to meet maxPlayEndPts.
         *
         * Once exceeded, the interjection will be interrupted to return to the main stream. This is used to decide what
         * to do if an interjection's playback is delayed somehow once it starts playing.
         *
         * If null, a playing interjection is never interrupted by this mechanism.
         */
        playingInterjectionPriorityTime: number | null;

        /**
         * The amount of time past maxPlayEndPts that interjections that have not started playing yet but are from an
         * interjection selection (that might contain more than one interjection) that has already been selected have
         * priority over the main stream if it stalls long enough to fail to meet maxPlayEndPts.
         *
         * No further interjections from the selection will start playback if they would end after this time.
         *
         * If null, a playing interjection selection is never interrupted by this mechanism.
         */
        interjectionSelectionPriorityTime: number | null;

        /**
         * The amount of time to spend starting the main/live stream up again and synchronizing it.
         */
        mainStreamWarmUpTime: number;

        /**
         * An object given by the caller of the API on the server.
         *
         * The user of the client library (usually) gives the client library a function to choose which interjection(s)
         * to show. This field is intended to be used by that function.
         */
        other?: any;
    }

    /**
     * Interjection metadata.
     *
     * This is generated with `set-interjection-metadata`. The documentation for that tool describes its format.
     */
    export interface Interjection {
        contentDuration: number;
        segmentDuration: number;
        video: {
            width: number;
            height: number;
            frameRate: [number, number];
            mimeType: string;
            codecs: string;
        }[];
        audio: {
            mimeType: string;
            codecs: string;
        }[];
        tag?: any;
        other?: any;
    }

    /**
     * The value of an interjection entry in an interjection set.
     */
    export interface InterjectionSetInterjection {
        duration: number;
        tag?: any;
    }

    /**
     * Interjection set metadata.
     *
     * This is generated with `generate-interjection-set-metadata`. The documentation for that tool describes its
     * format.
     */
    export interface InterjectionSet {
        interjections: {[key: string]: InterjectionSetInterjection};
        other?: any;
    }
}
