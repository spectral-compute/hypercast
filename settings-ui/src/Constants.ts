import {AudioVariant, VideoVariant, Channel, AudioCodec, StreamVariantConfig} from "./api/Config";
import {fuzzyMatch, FuzzyMatchResult, RecursivePartial} from "./Fuzzify";

export const RES_1080p = [1920, 1080] as const;
export const RES_4k = [3840, 2160] as const;
export const RES_720p = [1280, 720] as const;
export const RES_480p = [640, 480] as const;
export const RES_240p = [320, 240] as const;
export const RES_144p = [192, 144] as const;


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

export const DECKLINK_PORTS_ORDERED: string[] = [
    "DeckLink 8K Pro (4)",
    "DeckLink 8K Pro (2)",
    "DeckLink 8K Pro (3)",
    "DeckLink 8K Pro (1)"
];

// The settings you need to select a specific SDI port on the RISE box.
export const DECKLINK_PORT_SETTINGS:  {[K in DecklinkPort]: RecursivePartial<Channel>} = {
    ["1"]: {
        source: {url: DECKLINK_PORTS_ORDERED[0], arguments: ["-f", "decklink"]}
    },
    ["2"]: {
        source: {url: DECKLINK_PORTS_ORDERED[1], arguments: ["-f", "decklink"]},
    },
    ["3"]: {
        source: {url: DECKLINK_PORTS_ORDERED[2], arguments: ["-f", "decklink"]},
    },
    ["4"]: {
        source: {url: DECKLINK_PORTS_ORDERED[3], arguments: ["-f", "decklink"]}
    }
};

// What you get from pressing "new channel".
export function makeDefaultChannel(): Channel {
    return {
        ... DECKLINK_PORT_SETTINGS["1"],
        qualities: [],
        dash: {},
        history: {}
    } as Channel;
}

export function defaultVariantConfig(w: number, h: number): StreamVariantConfig {
    return {
        audio: defaultAudioVariantConfig(),
        video: defaultVideoVariantConfig(w, h),
        targetLatency: 1000,
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
