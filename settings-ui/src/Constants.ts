import {AudioVariant, VideoVariant} from "./api/Config";
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
        crf: 10
    },
    MEDIUM: {
        crf: 15
    },
    HIGH: {
        crf: 25,
    },
    VERY_HIGH: {
        crf: 35
    }
};

export const FUZZY_AUDIO_QUALITY_SETTINGS: {[K in FuzzyAudioQuality]: RecursivePartial<AudioVariant>} = {
    LOW: {
        bitrate: 100
    },
    MEDIUM: {
        bitrate: 500
    },
    HIGH: {
        bitrate: 5000
    }
};


export function fuzzyConfigMatch<EnumT extends {[k: string]: string, DEFAULT: string}, SubjectT>(
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

    if (allAbsent) {
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
