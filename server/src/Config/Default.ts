export const defaultVideoConfig = {
    bitrate: 3000,
    crf: 25,
    width: 1920,
    height: 1080,
    framerateNumerator: 30000,
    framerateDenominator: 1001,
    gop: 450,
    codec: "h264"
};

export const defaultAudioConfig = {
    bitrate: 48,
    codec: "aac"
};

export const defaultConfig = {
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
        interleavedDirectDashSegments: false,
        interleaveTimestampInterval: 100
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
        configs: [defaultVideoConfig],
        sources: [],
        timestamp: false
    }
};
