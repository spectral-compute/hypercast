
export type RecursivePartial<T> = {
    [P in keyof T]?:
    T[P] extends (infer U)[] ? RecursivePartial<U>[] :
        T[P] extends (object | undefined) ? RecursivePartial<T[P]> :
            T[P]
};

export enum FuzzyMatchResult {
    // All of the keys in the fuzzy config subobject are present in the config object, and match it.
    MATCH,

    // Some of the keys in the fuzzy config subobject are present, but set to different values.
    MISMATCH,

    // None of the keys from the fuzzy config subobject are found in the configuration object.
    ABSENT
}

/**
 * Recursively match a partial object against a subject object.
 * @param x
 * @param fuzz
 */
export function fuzzyMatch<T extends {[k: string]: any}>(x: T, fuzz: RecursivePartial<T>): FuzzyMatchResult {
    let foundKeys = false;

    for (const [k, v] of Object.entries(fuzz)) {
        if (x[k] == null) {
            continue;
        }

        foundKeys = true;

        if (typeof v == "object") {
            if (typeof x[k] != "object") {
                return FuzzyMatchResult.MISMATCH;
            }

            const b = fuzzyMatch(x[k], v);
            if (b == FuzzyMatchResult.MISMATCH) {
                return b;
            }
            if (b != FuzzyMatchResult.ABSENT) {
                foundKeys = true;
            }
        } else if (v != x[k]) {
            return FuzzyMatchResult.MISMATCH;
        }
    }

    if (!foundKeys) {
        return FuzzyMatchResult.ABSENT;
    }

    return FuzzyMatchResult.MATCH;
}

// Overwrite fields in `x` that are defined in `fuzz`, returning the new object.
// All modified subobjects are reallocated.
export function fuzzyApply<T>(x: T, fuzz: RecursivePartial<T>) {
    x = {...x};

    const keys = Object.keys(fuzz) as (keyof T)[];
    for (const k of keys) {
        const v = fuzz[k]! as T[typeof k];
        if (typeof v == "object") {
            if (typeof x[k] != "object") {
                x[k] = v;
            } else {
                x[k] = fuzzyApply(x[k], v);
            }
        } else if (v != x[k]) {
            x[k] = v;
        }
    }

    return x;
}
