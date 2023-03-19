/**
 * Type representing failures of `assertNonNull()`. This makes it convenient to catch
 * such failures.
 */
export class NullityAssertionError extends Error {}

/**
 * Asserts that the given object is non-null, narrowing an object to a non-null type.
 */
export function assertNonNull<T>(x: T, message?: string): asserts x is NonNullable<T> {
    // eslint-disable-next-line @typescript-eslint/dot-notation
    if (process.env["NODE_ENV"] === "production") {
        return;
    }

    if (x === null) {
        throw new NullityAssertionError(`Object asserted to be non-null is null${message ? ": " + message : "."}`);
    }

    if (x === undefined) {
        throw new NullityAssertionError(`Object asserted to be non-null is undefined${message ? ": " + message : "."}`);
    }
}

