import {StreamingConfig} from "./Config";

export interface BaseResponse {
    // Undefined if everything was OK.
    error?: string;
}

/// Turn streaming on/off
export interface SetStreamingStateRequest {
    active: boolean;
}
export type SetStreamingStateResponse = BaseResponse;


// Server should send the entire loaded config object
export interface GetCurrentConfigRequest {}
export interface GetCurrentConfigResponse extends BaseResponse {
    cfg: StreamingConfig;
}

/**
 * Represents an input media source.
 */
export interface Source
{
    /**
     * The URL to give ffmpeg after -i.
     */
    url: string,

    /**
     * The arguments to give ffmpeg before -i.
     */
    arguments: string[] | undefined
}
