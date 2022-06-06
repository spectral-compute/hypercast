import {expect, test} from "@jest/globals";
import {applyDefaultConfig} from "../src/Config/Config";

test("Default", (): void => {
    const config = applyDefaultConfig({});
    expect(config).toStrictEqual({
        audio: {
            configs: []
        },
        dash: {
            segmentLengthMultiplier: 1,
            rateControlBufferLength: 1000,
            manifest: "/live/{uid}/angle-{1}/manifest.mpd",
            targetLatency: 500,
            terminateDriftLimit: 0,
            interleave: true,
            interleaveTimestampInterval: 100,
            interleavedDirectDashSegments: false
        },
        network: {
            nonLiveCacheTime: 600,
            origin: "*",
            port: 8080,
            preAvailabilityTime: 4000,
            segmentRetentionCount: 6
        },
        serverInfo: {
            live: "/live/info.json"
        },
        video: {
            codecConfig: {
                av1Speed: 8,
                vp8Speed: 8,
                vp9Speed: 8,
                h264Preset: "faster",
                h265Preset: "faster"
            },
            configs: [
                {
                    bitrate: 3000,
                    crf: 25,
                    width: 1920,
                    height: 1080,
                    framerateNumerator: 30000,
                    framerateDenominator: 1001,
                    gop: 450,
                    codec: "h264"
                }
            ],
            sources: [],
            timestamp: false
        }
    });
});

test("Override", (): void => {
    const config = applyDefaultConfig({
        serverInfo: {
            live: "/live/live.json"
        }
    });
    expect(config).toStrictEqual({
        audio: {
            configs: []
        },
        dash: {
            segmentLengthMultiplier: 1,
            rateControlBufferLength: 1000,
            manifest: "/live/{uid}/angle-{1}/manifest.mpd",
            targetLatency: 500,
            terminateDriftLimit: 0,
            interleave: true,
            interleaveTimestampInterval: 100,
            interleavedDirectDashSegments: false
        },
        network: {
            nonLiveCacheTime: 600,
            origin: "*",
            port: 8080,
            preAvailabilityTime: 4000,
            segmentRetentionCount: 6
        },
        serverInfo: {
            live: "/live/live.json"
        },
        video: {
            codecConfig: {
                av1Speed: 8,
                vp8Speed: 8,
                vp9Speed: 8,
                h264Preset: "faster",
                h265Preset: "faster"
            },
            configs: [
                {
                    bitrate: 3000,
                    crf: 25,
                    width: 1920,
                    height: 1080,
                    framerateNumerator: 30000,
                    framerateDenominator: 1001,
                    gop: 450,
                    codec: "h264"
                }
            ],
            sources: [],
            timestamp: false
        }
    });
});

test("Two video configs", (): void => {
    const config = applyDefaultConfig({
        video: {
            configs: [
                {
                    bitrate: 2000
                },
                {
                    bitrate: 1000
                }
            ]
        }
    });
    expect(config).toStrictEqual({
        audio: {
            configs: []
        },
        dash: {
            segmentLengthMultiplier: 1,
            rateControlBufferLength: 1000,
            manifest: "/live/{uid}/angle-{1}/manifest.mpd",
            targetLatency: 500,
            terminateDriftLimit: 0,
            interleave: true,
            interleaveTimestampInterval: 100,
            interleavedDirectDashSegments: false
        },
        network: {
            nonLiveCacheTime: 600,
            origin: "*",
            port: 8080,
            preAvailabilityTime: 4000,
            segmentRetentionCount: 6
        },
        serverInfo: {
            live: "/live/info.json"
        },
        video: {
            codecConfig: {
                av1Speed: 8,
                vp8Speed: 8,
                vp9Speed: 8,
                h264Preset: "faster",
                h265Preset: "faster"
            },
            configs: [
                {
                    bitrate: 2000,
                    crf: 25,
                    width: 1920,
                    height: 1080,
                    framerateNumerator: 30000,
                    framerateDenominator: 1001,
                    gop: 450,
                    codec: "h264"
                },
                {
                    bitrate: 1000,
                    crf: 25,
                    width: 1920,
                    height: 1080,
                    framerateNumerator: 30000,
                    framerateDenominator: 1001,
                    gop: 450,
                    codec: "h264"
                }
            ],
            sources: [],
            timestamp: false
        }
    });
});
