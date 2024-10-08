export enum LogLevel {
    debug = "debug",
    info = "info",
    warning = "warning",
    error = "error",
    fatal = "fatal"
}

export enum VideoCodec {
    h264 = "h264",
    h265 = "h265",
    vp8 = "vp8",
    vp9 = "vp9",
    av1 = "av1"
}

export enum AudioCodec {
    none = "none",
    aac = "aac",
    opus = "opus"
}

export enum FrameRateType {
    fps = "fps",
    fraction = "fraction",
    fraction23 = "fraction23"
}

export enum H26xPreset {
    ultrafast = "ultrafast",
    superfast = "superfast",
    veryfast = "veryfast",
    faster = "faster",
    fast = "fast",
    medium = "medium",
    slow = "slow",
    slower = "slower",
    veryslow = "veryslow",
    placebo = "placebo"
}

export interface MediaSource {
    url: string;
    arguments: string[];
    loop: boolean;
    timestamp: boolean;

    // Latency introduced by the source hardware itself, if known.
    latency?: number | null;
}

export interface ClientBufferControl {
    extraBuffer?: number | null;
    initialBuffer?: number | null;
    seekBuffer?: number | null;
    minimumInitTime?: number | null;
}

export interface FrameRate {
    type: FrameRateType,
    numerator: number;
    denominator: number;
}

export type FrameRateSpecial = undefined | "half" | "half+";
export type FrameRateCfg = FrameRate | FrameRateSpecial;

export interface VideoVariant {
    width: number;
    height: number;
    frameRate?: FrameRateCfg;
    bitrate?: number | null;
    minBitrate?: number | null;
    crf?: number;
    rateControlBufferLength?: number | null;
    codec?: VideoCodec;
    h26xPreset?: H26xPreset;
    vpXSpeed?: number | null;
    gop?: number | null;
}


export interface AudioVariant {
    sampleRate?: number | null;
    bitrate: number;
    codec: AudioCodec;
}

export interface StreamVariantConfig {
    // The audio/video variants selected for this stream.
    video: VideoVariant;
    audio: AudioVariant;

    targetLatency: number;
    minInterleaveRate?: number;
    minInterleaveWindow?: number;
    interleaveTimestampInterval?: number;
    clientBufferControl?: ClientBufferControl;
}


export interface Channel {
    source: MediaSource;

    // Define stream variants that exist.
    qualities: StreamVariantConfig[];
    dash: DashConfig;
    history: HistoryConfig;
}

export interface HttpConfig {
    origin?: string | null;
    cacheNonLiveTime?: number;
}

export interface DashConfig {
    segmentDuration?: number;
    expose?: boolean;
    preAvailabilityTime?: number;
}

export interface NetworkConfig {
    port?: number;
    transitLatency?: number;
    transitJitter?: number;
    transitBufferSize?: number;
}

export interface HistoryConfig {
    historyLength?: number;
}

export interface Directory {
    localPath: string;
    index: string;
    secure: boolean;
    ephemeral: boolean;
}

export interface PathsConfig {
    liveInfo: string;
    liveStream: string;
    directories: {[key: string]: Directory};
}

export interface LogConfig {
    path?: string;
    print?: boolean | null;
    level: LogLevel;
}

export interface StreamingConfig {
    channels: {[name: string]: Channel};
    network: NetworkConfig;
    http: HttpConfig;
    paths: PathsConfig;
    log: LogConfig;
}
