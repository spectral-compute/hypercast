import {Api} from "./api/Api";
import {MachineInfo, PortConnector} from "./api/Hardware";
import {AudioCodec, Channel, FrameRateType, H26xPreset, LogLevel, StreamingConfig, VideoCodec} from "./api/Config";


const chan: Channel = {
    videoSource: {
        arguments: [],
        latency: 0,
        url: "herring",
        loop: true,
        timestamp: false
    },

    videoVariants: {
        Toast: {
            width: 1920,
            height: 1080,
            frameRate: {
                numerator: 30,
                denominator: 1,
                type: FrameRateType.fps
            },
            bitrate: 10000,
            crf: 5,
            codec: VideoCodec.h264,
            h26xPreset: H26xPreset.fast
        },
        otherOne: {
            width: 1280,
            height: 720,
            frameRate: {
                numerator: 30,
                denominator: 1,
                type: FrameRateType.fps
            },
            bitrate: 10000,
            crf: 5,
            codec: VideoCodec.h264,
            h26xPreset: H26xPreset.fast
        }
    },
    audioVariants: {
        Toast: {
            bitrate: 1000,
            codec: AudioCodec.aac
        },
        otherOne: {
            bitrate: 1000,
            codec: AudioCodec.aac
        }
    },

    streams: {
        nice: {
            audio: "Toast",
            video: "Toast",
            targetLatencyMs: 1000
        },
        crap: {
            audio: "otherOne",
            video: "otherOne",
            targetLatencyMs: 3000
        },
    }
};

const channels: {[name: string]: Channel} = {
    "Channel 1": chan,
    "Channel 2": chan
};

export class AppCtx {
    api: Api = new Api();

    // The server's config object.
    loadedConfiguration: StreamingConfig = {
        channels,
        dash: {},
        network: {},
        paths: {
            liveInfo: "Hi",
            liveStream: "There",
            directories: {}
        },
        history: {},
        log: {
            level: LogLevel.debug
        },
        http: {}
    };

    // The current status of the hardware.
    machineInfo: MachineInfo = {
        isStreaming: false,
        inputPorts: [{
            name: "SDI 1",
            connector: PortConnector.SDI,
            connectedMediaInfo: {
                framerateDenominator: 30,
                framerateNumerator: 1,
                height: 1080,
                width: 1920,
            }
        }, {
            name: "SDI 2",
            connector: PortConnector.SDI,
        }, {
            name: "SDI 3",
            connector: PortConnector.SDI,
        }, {
            name: "SDI 4",
            connector: PortConnector.SDI,
        },
        ]
    };

    constructor() {
    }
}
