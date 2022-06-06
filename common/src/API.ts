/**
 * API shared between the server and the client.
 */
export namespace API {
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
        height: number
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
}
