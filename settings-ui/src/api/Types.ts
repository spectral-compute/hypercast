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
