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

/**
 * Wait for an event in an EventTarget to fire.
 *
 * @param type The event type to wait for.
 * @param target The EventTarget to wait to emit the event.
 * @param signal The abort signal to reject on.
 */
export function waitForEvent(type: string, target: EventTarget, signal: AbortSignal | null = null): Promise<void> {
    return new Promise<void>((resolve, reject) => {
        // Reject (i.e: throw an exception) if the abort is triggered.
        const onAbort = ((reject) => (): void => {
            const e = new Error();
            e.name = "AbortError";
            reject(e);
        })(reject);
        if (signal !== null) {
            signal.addEventListener("abort", onAbort, {once: true});
        }

        // Finish waiting once playback has resumed.
        const onResolve = ((resolve, signal, onAbort) => (): void => {
            if (signal) {
                signal.removeEventListener("abort", onAbort);
            }
            resolve();
        })(resolve, signal, onAbort);
        target.addEventListener(type, onResolve, {once: true, signal: signal ?? undefined});
    });
}
