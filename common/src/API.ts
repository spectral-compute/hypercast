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
     * Interface for the `angles` field of the ServerInfo.
     */
    export interface AngleConfig {
        name: string;
        path: string;
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
     * Main server information object.
     *
     * This is exposed as `/live/live.json` by default.
     */
    export interface ServerInfo {
        angles: AngleConfig[],
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
        userJsonObject = 48,
        userBinaryData = 49,
        userString = 50,
        discard = 255
    }
}
