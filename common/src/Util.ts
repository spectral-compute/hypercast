/**
 * Wait (and suspend the coroutine) for a given length of time.
 *
 * @param ms The amount of time to wait for.
 * @param signal The abort signal to check.
 */
export function sleep(ms: number, signal: AbortSignal | null = null): Promise<void> {
    return new Promise<void>((resolve, reject) => setTimeout(((resolve, reject, signal) => (): void => {
        if (signal?.aborted) {
            const e = new Error();
            e.name = "AbortError";
            reject(e);
        }
        resolve();
    })(resolve, reject, signal), ms));
}
