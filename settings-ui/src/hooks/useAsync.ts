import {useCallback, useEffect, useRef, useState} from "react";

export interface AsyncHookStatus {
    // The current status of the async function.
    status: StatusString;
    isLoading: boolean; // `status == "pending"`.
    isFulfilled: boolean; // `status == "fulfilled"`.

    // Contains the error from the most recent rejection, else undefined.
    error: Error | undefined;

    // When it finished, if it has finished.
    finishedAt: Date | undefined;
}
export interface AsyncHookResponse<ArgsT extends any[], ResultT> extends AsyncHookStatus {
    // Run the async function again with new arguments. Not allowed if the status is "pending".
    run: (...params: ArgsT) => void;

    // If status is "fulfilled", this contains the result of the async function.
    // If the function is run again, both this and `error` will retain the results of the previous invocation until the
    // new one finishes.
    data: ResultT | undefined;
}

export interface AsyncHookBonusOptions<ArgsT, ResultT> {
    // Function to call with the result data, when it becomes available. Mostly useful for toasts.
    onComplete?: (args: ArgsT, data: ResultT) => void;

    // Function to call with any error that arises. Mostly useful for toasts.
    onError?: (args: ArgsT, e: Error) => void;
}

// If you want to run it now, you must provide the arguments. Otherwise, you do not.
// The behaviour is undefined if:
// - The value of `runNow` is changed between renders
// - The function is changed between renders (reference identity may change, but if you try to swap in a different
//   function you're gonna have a bad time.
// - The function depends on any non-global, non-react-context closure-captured values.
export function useAsync<ArgsT extends any[], ResultT>(fn: (...args: ArgsT) => Promise<ResultT>, options: AsyncHookBonusOptions<ArgsT, ResultT>, runNow: true, ...args: ArgsT): AsyncHookResponse<ArgsT, ResultT>;
export function useAsync<ArgsT extends any[], ResultT>(fn: (...args: ArgsT) => Promise<ResultT>, options: AsyncHookBonusOptions<ArgsT, ResultT>, runNow: false): AsyncHookResponse<ArgsT, ResultT>;
export function useAsync<ArgsT extends any[], ResultT>(fn: (...args: ArgsT) => Promise<ResultT>, options: AsyncHookBonusOptions<ArgsT, ResultT>, runNow: boolean, ...args: ArgsT): AsyncHookResponse<ArgsT, ResultT> {
    const [status, setStatus] = useState<StatusString>(runNow ? "pending" : "initial");
    const [data, setData] = useState<ResultT | undefined>(undefined);
    const [error, setError] = useState<Error | undefined>(undefined);
    const [finishedAt, setFinishedAt] = useState<Date | undefined>(undefined);
    const [theFunc, _nope] = useState<(...args: ArgsT) => Promise<ResultT>>(() => fn);

    // Resist double-invocation of immediate async tasks if the component is remounted.
    const started = useRef("");

    const run = useCallback(async (...cArgs: ArgsT): Promise<void> => {
        setStatus("pending");
        return theFunc(...cArgs)
            .then((response) => {
                setData(response);
                setStatus("fulfilled");
                setFinishedAt(new Date());
                if (options.onComplete) {
                    options.onComplete(cArgs, response);
                }
            }).catch((error) => {
                console.error(error);
                setError(error);
                setStatus("rejected");
                if (options.onError) {
                    options.onError(cArgs, error);
                }
            });
    }, [theFunc]);

    const argsJSON = JSON.stringify(args);
    useEffect(() => {
        if (runNow && started.current !== argsJSON) {
            started.current = argsJSON;
            void run(...args);
        }
    }, [argsJSON]);

    return {
        status,
        isLoading: (status == "pending"),
        isFulfilled: (status == "fulfilled"),
        run,
        data,
        error,
        finishedAt
    };
}

export function useAsyncImmediateEx<ArgsT extends any[], ResultT>(fn: (...args: ArgsT) => Promise<ResultT>, options: AsyncHookBonusOptions<ArgsT, ResultT>, ...args: ArgsT): AsyncHookResponse<ArgsT, ResultT> {
    return useAsync(fn, options, true, ...args);
}
export function useAsyncDeferredEx<ArgsT extends any[], ResultT>(fn: (...args: ArgsT) => Promise<ResultT>, options: AsyncHookBonusOptions<ArgsT, ResultT>): AsyncHookResponse<ArgsT, ResultT> {
    return useAsync(fn, options, false);
}

export function useAsyncImmediate<ArgsT extends any[], ResultT>(fn: (...args: ArgsT) => Promise<ResultT>, ...args: ArgsT): AsyncHookResponse<ArgsT, ResultT> {
    return useAsync(fn, {}, true, ...args);
}
export function useAsyncDeferred<ArgsT extends any[], ResultT>(fn: (...args: ArgsT) => Promise<ResultT>): AsyncHookResponse<ArgsT, ResultT> {
    return useAsync(fn, {}, false);
}

export enum StatusTypes {
    initial = "initial",
    pending = "pending",
    fulfilled = "fulfilled",
    rejected = "rejected"
}
export type StatusString = keyof typeof StatusTypes;

// Combine two status codes together.
export function combineStatuses(s1: StatusString, s2: StatusString): StatusTypes {
    if (s1 == StatusTypes.rejected || s2 == StatusTypes.rejected) {
        return StatusTypes.rejected;
    }

    if (s1 == StatusTypes.pending || s2 == StatusTypes.pending) {
        return StatusTypes.pending;
    }

    if (s1 == StatusTypes.initial || s2 == StatusTypes.initial) {
        return StatusTypes.initial;
    }

    if (s1 == StatusTypes.fulfilled && s2 == StatusTypes.fulfilled) {
        return StatusTypes.fulfilled;
    }

    return StatusTypes.pending;
}
