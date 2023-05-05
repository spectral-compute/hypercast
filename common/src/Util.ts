/// Make an AbortError
export function mkAbortError() {
    const e = new Error();
    e.name = "AbortError";
    return e;
}

/**
 * Create a promise that rejects (with an AbortError) when a signal aborts, but otherwise is equivalent to new Promise.
 *
 * @param resolve The resolution function that would normally be passed to Promise's constructor.
 * @param signal Reject if this signal aborts.
 */
export function abortablePromise<T>(resolve: (fulfill: (value: T) => void, reject: (e: Error) => void) => void,
                                    signal: AbortSignal): Promise<T> {
    return new Promise<T>((fulfill, reject) => {
        /* Make abort signal call reject if it aborts. */
        const onAbort = () => {
            reject(mkAbortError());
        };
        signal.addEventListener("abort", onAbort);

        /* Give the caller a fulfillment function that removes the event listener and calls the promise's fulfillment
           function. */
        const onFulfill = (value: T) => {
            signal.removeEventListener("abort", onAbort);
            fulfill(value);
        };

        /* Give the caller a rejection function that removes the event listener and calls the promise's rejection
           function. */
        const onReject = (e: Error) => {
            signal.removeEventListener("abort", onAbort);
            reject(e);
        };

        /* Pass the above promise resolution functions to the caller. */
        resolve(onFulfill, onReject);
    });
}

/**
 * Wait (and suspend the coroutine) for a given length of time.
 *
 * @param ms The amount of time to wait for.
 * @param signal The abort signal to check.
 */
export function sleep(ms: number, signal: AbortSignal | null = null): Promise<void> {
    return new Promise<void>((resolve, reject) => {
        setTimeout(() => {
            if (signal?.aborted) {
                reject(mkAbortError());
                return;
            }

            resolve();
        }, ms);
    });
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
        const onAbort = () => {
            reject(mkAbortError());
        };

        if (signal != null) {
            signal.addEventListener("abort", onAbort, {once: true});
        }

        // Finish waiting once playback has resumed.
        const onResolve = () => {
            if (signal) {
                signal.removeEventListener("abort", onAbort);
            }
            resolve();
        };
        target.addEventListener(type, onResolve, {once: true, ...(signal ? {signal} : {})});
    });
}
