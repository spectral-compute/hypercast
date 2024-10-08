import {AudioVariant, VideoVariant, Channel, AudioCodec, StreamVariantConfig} from "./api/Config";
import {fuzzyMatch, FuzzyMatchResult, RecursivePartial} from "./Fuzzify";

export const RES_4k = [3840, 2160] as const;
export const RES_1080p = [1920, 1080] as const;
export const RES_720p = [1280, 720] as const;
export const RES_360p = [640, 360] as const;


export type FuzzyQualityEnum<T> = Exclude<keyof T, "DEFAULT">;

// Prettymuch an enum, but weirder. Defines the different levels that exist for a specific type of fuzzy config.
// If the config object contains no values for any of the keys used by any of the fuzzy levels (defined below) for
// one of these enums, it is interpreted as being equal to whatever level corresponds to "DEFAULT" here.
// Otherwise, if it has keys but some of them mismatch, then it is considered to match no level.
export const FUZZY_AUDIO_QUALITIES = {
    LOW: "LOW",
    MEDIUM: "MEDIUM",
    HIGH: "HIGH",

    DEFAULT: "MEDIUM"
} as const;
export type FuzzyAudioQuality = FuzzyQualityEnum<typeof FUZZY_AUDIO_QUALITIES>;

export const FUZZY_VIDEO_QUALITIES = {
    LOW: "LOW",
    MEDIUM: "MEDIUM",
    HIGH: "HIGH",
    VERY_HIGH: "VERY_HIGH",

    DEFAULT: "MEDIUM"
} as const;
export type FuzzyVideoQuality = FuzzyQualityEnum<typeof FUZZY_VIDEO_QUALITIES>;

// Define the fuzzy video quality presets in terms of partial variant settings objects.
// Any subset of the keys from the VideoVariant object can be specified here.
export const FUZZY_VIDEO_QUALITY_SETTINGS: {[K in FuzzyVideoQuality]: RecursivePartial<VideoVariant>} = {
    LOW: {
        crf: 30
    },
    MEDIUM: {
        crf: 26
    },
    HIGH: {
        crf: 22,
    },
    VERY_HIGH: {
        crf: 18
    }
};

export const FUZZY_AUDIO_QUALITY_SETTINGS: {[K in FuzzyAudioQuality]: RecursivePartial<AudioVariant>} = {
    LOW: {
        bitrate: 48
    },
    MEDIUM: {
        bitrate: 96
    },
    HIGH: {
        bitrate: 192
    }
};


export const DECKLINK_PORT = {
    ["1"]: "1",
    ["2"]: "2",
    ["3"]: "3",
    ["4"]: "4",
} as const;
export type DecklinkPort = FuzzyQualityEnum<typeof DECKLINK_PORT>;

export const DECKLINK_PORTS_ORDERED: DecklinkPort[] = [
    "1",
    "2",
    "3",
    "4"
];

// The settings you need to select a specific SDI port on the RISE box.
export const DECKLINK_PORT_SETTINGS:  {[K in DecklinkPort]: RecursivePartial<Channel>} = {
    ["1"]: {
        source: {url: "DeckLink 8K Pro (4)", arguments: ["-f", "decklink"]}
    },
    ["2"]: {
        source: {url: "DeckLink 8K Pro (1)", arguments: ["-f", "decklink"]},
    },
    ["3"]: {
        source: {url: "DeckLink 8K Pro (3)", arguments: ["-f", "decklink"]},
    },
    ["4"]: {
        source: {url: "DeckLink 8K Pro (2)", arguments: ["-f", "decklink"]}
    }
};

// What you get from pressing "new channel".
export function makeDefaultChannel(input: DecklinkPort | null): Channel {
    return {
        ... (input != null ? DECKLINK_PORT_SETTINGS[input] : {}),
        qualities: [],
        dash: {},
        history: {}
    } as Channel;
}

export function defaultVariantConfig(w: number, h: number): StreamVariantConfig {
    return {
        audio: defaultAudioVariantConfig(),
        video: defaultVideoVariantConfig(w, h),
        targetLatency: (w < 1280 || h < 720) ? 2000 : 1000,
    };
}

export function defaultAudioVariantConfig(): AudioVariant {
    return {
        codec: AudioCodec.aac,
        ...FUZZY_AUDIO_QUALITY_SETTINGS[FUZZY_AUDIO_QUALITIES.DEFAULT] as any
    };
}

export function defaultVideoVariantConfig(w: number, h: number): VideoVariant {
    return {
        width: w,
        height: h,
        ...FUZZY_VIDEO_QUALITY_SETTINGS[FUZZY_VIDEO_QUALITIES.DEFAULT]
    };
}


export function fuzzyConfigMatch<EnumT extends {[k: string]: string | undefined, DEFAULT?: string}, SubjectT>(
    theEnum: EnumT,
    subject: SubjectT,
    fuzzinessCfgObject: {[K in keyof EnumT]?: RecursivePartial<SubjectT>}
): FuzzyQualityEnum<EnumT> | null {
    let allAbsent = true;

    const keys = Object.keys(fuzzinessCfgObject) as (keyof typeof fuzzinessCfgObject)[];
    for (const k of keys) {
        const v = fuzzinessCfgObject[k]!;
        let r = fuzzyMatch(subject, v);
        if (r == FuzzyMatchResult.MATCH) {
            return k as FuzzyQualityEnum<EnumT>;
        }

        allAbsent &&= r == FuzzyMatchResult.ABSENT;
    }

    if (allAbsent && theEnum["DEFAULT"] != null) {
        return theEnum["DEFAULT"] as FuzzyQualityEnum<EnumT>;
    }

    return null;
}

export function fuzzyVideoQualityMatch(subject: VideoVariant) {
    return fuzzyConfigMatch(FUZZY_VIDEO_QUALITIES, subject, FUZZY_VIDEO_QUALITY_SETTINGS);
}
export function fuzzyAudioQualityMatch(subject: AudioVariant) {
    return fuzzyConfigMatch(FUZZY_AUDIO_QUALITIES, subject, FUZZY_AUDIO_QUALITY_SETTINGS);
}

export function fuzzyInputMatch(subject: Channel) {
    return fuzzyConfigMatch(DECKLINK_PORT, subject, DECKLINK_PORT_SETTINGS);
}

export const SELF_TEST_CHANNELS: {[name: string]: Channel} = {
    "live/Channel-0": {
        source: {
            url: DECKLINK_PORT_SETTINGS["1"].source!.url!,
            arguments: DECKLINK_PORT_SETTINGS["1"].source!.arguments!,
            loop: false,
            timestamp: false
        },
        qualities: [{
            audio: {codec: AudioCodec.aac, bitrate: 96},
            video: {width: 640, height: 360, crf: 26},
            targetLatency: 2000
        }],
        dash: {},
        history: {}
    },
    "live/Channel-1": {
        source: {
            url: DECKLINK_PORT_SETTINGS["2"].source!.url!,
            arguments: DECKLINK_PORT_SETTINGS["2"].source!.arguments!,
            loop: false,
            timestamp: false
        },
        qualities: [{
            audio: {codec: AudioCodec.aac, bitrate: 96},
            video: {width: 640, height: 360, crf: 26},
            targetLatency: 2000
        }],
        dash: {},
        history: {}
    },
    "live/Channel-2": {
        source: {
            url: DECKLINK_PORT_SETTINGS["3"].source!.url!,
            arguments: DECKLINK_PORT_SETTINGS["3"].source!.arguments!,
            loop: false,
            timestamp: false
        },
        qualities: [{
            audio: {codec: AudioCodec.aac, bitrate: 96},
            video: {width: 640, height: 360, crf: 26},
            targetLatency: 2000
        }],
        dash: {},
        history: {}
    },
    "live/Channel-3": {
        source: {
            url: DECKLINK_PORT_SETTINGS["4"].source!.url!,
            arguments: DECKLINK_PORT_SETTINGS["4"].source!.arguments!,
            loop: false,
            timestamp: false
        },
        qualities: [{
            audio: {codec: AudioCodec.aac, bitrate: 96},
            video: {width: 640, height: 360, crf: 26},
            targetLatency: 2000
        }],
        dash: {},
        history: {}
    },
};
