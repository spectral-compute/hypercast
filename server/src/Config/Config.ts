import {readFileSync} from "fs";
import * as is from "typescript-is";
import * as ckis from "@ckitching/typescript-is";
import {assertNonNull} from "live-video-streamer-common";

import {Config, VideoConfig} from "./Spec";
import {defaultConfig, defaultVideoConfig, defaultAudioConfig} from "./Default";
export * from "./Spec";

export function computeSegmentDuration(segmentLengthMultiplier: number,
                                       videoConfigs: VideoConfig[]): [number, number] {
    // Some utility functions.
    const gcd = (a: number, b: number): number => {
        if (b === 0) {
            return a;
        }
        return gcd(b, a % b); // Yay for Euclid's algorithm.
    };
    const lcm = (a: number, b: number): number => {
        return (a * b) / gcd(a, b);
    };

    // Figure out a common time-base. The fraction we're computing is a duration. But we're computing it based on frame
    // rates, which are a reciprocal time period.
    let commonGopPeriodDenominator = 1;
    for (const config of videoConfigs) {
        commonGopPeriodDenominator = lcm(commonGopPeriodDenominator, config.framerateNumerator);
    }

    // Figure out the time, in the common time-base, that is the smallest integer multiple of every GOP.
    let commonGopPeriodNumerator = 1;
    for (const config of videoConfigs) {
        const configGopPeriodNumerator =
            config.framerateDenominator * commonGopPeriodDenominator / config.framerateNumerator;
        commonGopPeriodNumerator = lcm(configGopPeriodNumerator * config.gop, commonGopPeriodNumerator);
    }

    // Multiply that by the segment length multiplier.
    commonGopPeriodNumerator *= segmentLengthMultiplier;
    const simplify = gcd(commonGopPeriodNumerator, commonGopPeriodDenominator);
    commonGopPeriodNumerator /= simplify;
    commonGopPeriodDenominator /= simplify;

    // Done :)
    return [commonGopPeriodNumerator, commonGopPeriodDenominator];
}

export function substituteManifestPattern(pattern: string, uniqueId: string, index?: number): string {
    let result = pattern.replace(/{uid}/g, uniqueId);
    while (true) {
        const match = /[{]([0-9]+)[}]/.exec(result);
        if (match === null) {
            break;
        }

        assertNonNull(match);
        assertNonNull(match.index);

        const startIdx = match.index;
        const endIdx = match.index + match[0]!.length;
        const numDigits = parseInt(match[1]!);

        const replacement = (index === undefined) ?
                            (`[0-9]{${numDigits},}`) : index.toString().padStart(numDigits, "0");
        result = result.substr(0, startIdx) + replacement + result.substr(endIdx);
    }
    return result;
}

/* Apply an Object.assign(...dst, ...src) like operation, but recurse into objects. */
function apply(dst: unknown, src: unknown): unknown {
    if (src === undefined) {
        return dst;
    }

    if (!(src instanceof Object) || !(dst instanceof Object) || dst instanceof Array) {
        return src;
    }

    const keys = Object.keys(dst);
    if (keys.length === 0) {
        return src;
    }

    const result: unknown = {};
    for (const key of keys) {
        Object.defineProperty(result, key, {
            value: apply(Object.getOwnPropertyDescriptor(dst, key)!.value,
                         Object.getOwnPropertyDescriptor(src, key)?.value),
            enumerable: true,
            writable: true
        });
    }
    for (const key of Object.keys(src)) {
        if (keys.includes(key)) {
            continue;
        }
        Object.defineProperty(result, key, {
            value: Object.getOwnPropertyDescriptor(src, key)!.value as unknown,
            enumerable: true,
            writable: true
        });
    }
    return result;
}

export function applyDefaultConfig(config: unknown): Config {
    /* Apply defaults for the configuration. */
    // The following apply() imposes a superset of the following keys (needed later) on the result.
    interface ConfigPart {
        video: {
            configs: unknown[]
        },
        audio: {
            configs: unknown[]
        }
    }

    // Most of the configuration.
    const result = apply(defaultConfig, config) as ConfigPart;

    // Video configuration.
    for (let i = 0; i < result.video.configs.length; i++) {
        result.video.configs[i] = apply(defaultVideoConfig, result.video.configs[i]);
    }

    // Audio configuration.
    for (let i = 0; i < result.audio.configs.length; i++) {
        result.audio.configs[i] = apply(defaultAudioConfig, result.audio.configs[i]);
    }

    if (!ckis.equals<Config>(result)) {
        try {
            is.assertEquals<Config>(result);
            throw new Error("Invalid configuration.");
        } catch (e) {
            throw new Error(`Invalid configuration: ${(e as Error).message}`);
        }
    }
    return result;
}

export function loadConfig(path: string): Config {
    const json: string = readFileSync(path, {encoding: "utf-8"});
    return applyDefaultConfig(JSON.parse(json));
}
