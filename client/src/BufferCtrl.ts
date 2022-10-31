import {TimestampInfo} from "./Deinterleave";
import {ReceivedInfo} from "./Stream";
import {BufferControlTickInfo, DebugHandler} from "./Debug"

export interface NetworkTimingStats {
    delayMean: number,
    delayMedian: number,
    delayMin: number,
    delayMax: number,
    delayPercentile10: number,
    delayPercentile90: number,
    delayStdDev: number,
    historyLength: number,
    historyAge: number
}

export interface CatchUpStats {
    eventCount: number,
    averageTimeBetweenEvents: number
}

/**
 * Control player buffer levels, and accelerate the video a bit if the buffer gets too big.
 */
export class BufferControl {
    /**
     * Control the buffer level of an HTMLMediaElement, and keep secondary media elements in sync.
     *
     * @param mediaElement The HTML media element to control the buffer level of.
     * @param verbose If true, some debug information is printed.
     * @param debugHandler The object to handle performance and debugging information.
     */
    constructor(mediaElement: HTMLMediaElement, verbose: boolean, debugHandler: DebugHandler | null = null) {
        this.mediaElement = mediaElement;
        this.verbose = verbose;
        if (process.env.NODE_ENV === "development") {
            if (debugHandler !== null) {
                debugHandler.setBufferControl(this);
                this.debugHandler = debugHandler;
            }
        }

        this.setLowLatency();
    }

    /**
     * Start the buffer control.
     */
    start(): void {
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;

        this.interval = setInterval((): void => {
            this.bufferControlTick();
        }, this.syncClockPeriod);
    }

    /**
     * Stop controlling anything.
     */
    stop(): void {
        if (this.interval) {
            clearInterval(this.interval);
        }
    }

    /**
     * To be called when the deinterleaver generates a timestamp information object.
     *
     * @param timestampInfo The timestamp information object from the deinterleaver.
     */
    onTimestamp(timestampInfo: TimestampInfo): void {
        const sliceStart: number = this.timestampInfos.findIndex((e: TimestampInfo): boolean => {
            return e.sentTimestamp >= timestampInfo.sentTimestamp - this.timestampInfoMaxAge; // Keep those new enough.
        });
        this.timestampInfos = (sliceStart < 0) ? [] : this.timestampInfos.slice(sliceStart, this.timestampInfos.length);
        this.timestampInfos.push(timestampInfo);

        if (process.env.NODE_ENV === "development") {
            if (this.debugHandler !== null) {
                this.debugHandler.onTimestamp(timestampInfo);
            }
        }
    }

    /**
     * To be called when new data is received from the stream.
     *
     * @param receivedInfo Information about the received data.
     */
    onRecieved(receivedInfo: ReceivedInfo) {
        if (process.env.NODE_ENV === "development") {
            if (this.debugHandler !== null) {
                this.debugHandler.onReceived(receivedInfo);
            }
        }
    }

    /**
     * Get the length, in milliseconds, of the buffer in one of the media elements.
     *
     * @return The buffer length, in seconds, of the media element.
     */
    getBufferLength(): number {
        const mediaElement = this.mediaElement;
        const buffered = mediaElement!.buffered;
        if (buffered.length === 0) {
            return NaN;
        }
        return (buffered.end(buffered.length - 1) - this.mediaElement.currentTime) * 1000;
    }

    // Automatically determine buffering.
    setAutoLatency(): void {
        this.autoBuffer = true;
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;
    }

    // Should get 3 seconds overall. Intended for the minimum quality to cope with poor connections/CDN buffering.
    setSaferLatency(): void {
        this.autoBuffer = false;
        this.maxBuffer = 1750;
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;
    }

    // Should get 2 seconds overall.
    setLowLatency(): void {
        this.autoBuffer = false;
        this.maxBuffer = 1000;
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;
    }

    // Should get 1 second overall.
    setUltraLowLatency(): void {
        this.autoBuffer = false;
        this.maxBuffer = 100;
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;
    }

    /**
     * Get the current buffer targets.
     */
    getBufferTarget(): number {
        return this.maxBuffer;
    }

    /**
     * Get the catch up event statistics.
     */
    getCatchUpStats(): CatchUpStats {
        return {
            eventCount: this.catchUpEvents,
            averageTimeBetweenEvents: (Date.now() - this.catchUpEventsStart) / this.catchUpEvents
        };
    }

    /**
     * Get network timing statistics.
     */
    getNetworkTimingStats(firstForInterleaveOnly: boolean = false): NetworkTimingStats {
        const delays: number[] = this.getSortedTimestampDelays(firstForInterleaveOnly);
        const length = delays.length;
        if (length === 0) {
            delays.push(NaN);
        }

        const mean: number = delays.reduce((a: number, b: number): number => a + b) / delays.length;
        const variance: number = delays.map((e: number): number => (mean - e) ** 2).
                                        reduce((a: number, b: number): number => a + b) / delays.length;

        return {
            delayMean: mean,
            delayMedian: delays[Math.floor(delays.length / 2)]!,
            delayMin: delays[0]!,
            delayMax: delays[delays.length - 1]!,
            delayPercentile10: delays[Math.floor(delays.length / 10)]!,
            delayPercentile90: delays[Math.floor(delays.length * 9 / 10)]!,
            delayStdDev: Math.sqrt(variance),
            historyLength: length,
            historyAge: ((this.timestampInfos.length < 2) ? 0 :
                        (this.timestampInfos[this.timestampInfos.length - 1]!.sentTimestamp -
                         this.timestampInfos[0]!.sentTimestamp))
        };
    }

    private bufferControlTick(): void {
        const now = Date.now().valueOf();
        const bufferLength = this.getBufferLength();
        let tickInfo: BufferControlTickInfo = {
            timestamp: now,
            primaryBufferLength: bufferLength,
            catchUp: false
        };

        /* Figure out the target buffer size. */
        if (this.autoBuffer) {
            const delays: number[] = this.getSortedTimestampDelays();
            if (delays.length < 100) {
                this.maxBuffer = 1000;
            } else {
                this.maxBuffer = delays[Math.floor(delays.length * this.timestampAutoBufferMax)]! - delays[0]! +
                                 this.timestampAutoBufferExtra;
                this.maxBuffer = Math.max(this.maxBuffer, this.timestampAutoMinTarget);
            }
        }

        /* If we have sufficiently little buffer that we don't need to skip. */
        if (bufferLength <= this.maxBuffer) {
            this.bufferExceededCount = 0;
            this.debugBufferControlTick(tickInfo);
            return;
        }

        /* Give the seeking a chance to catch up if we've *just* done a seek. */
        if (this.lastCatchUpEventClusterEnd >= now) {
            this.bufferExceededCount = 0;
            this.debugBufferControlTick(tickInfo);
            return;
        }

        /* We're far enough behind to want to skip ahead. */
        if (this.bufferExceededCount < this.bufferExceedTickCounts) {
            this.bufferExceededCount++;
            return;
        }
        this.bufferExceededCount = 0;

        const seekRange = this.mediaElement.seekable;
        if (seekRange.length === 0) {
            if (this.verbose) {
                console.warn("Cannot seek to catch up");
            }
            this.debugBufferControlTick(tickInfo);
            return;
        }
        this.mediaElement.currentTime = seekRange.end(seekRange.length - 1);
        this.lastCatchUpEventClusterEnd = now + this.catchUpEventDuration;
        if (this.lastCatchUpEventClusterEnd !== 0) {
            this.catchUpEvents++; // Not just the initial seek that always has to happen.
            tickInfo.catchUp = true;
        }

        this.debugBufferControlTick(tickInfo);
        return;
    }

    private debugBufferControlTick(tickInfo: BufferControlTickInfo): void {
        if (process.env.NODE_ENV !== "development") {
            return;
        }
        if (this.debugHandler === null) {
            return;
        }
        this.debugHandler.onBufferControlTick(tickInfo);
    }

    /**
     * Get a sorted array of delays.
     */
    private getSortedTimestampDelays(firstForInterleaveOnly: boolean = false): number[] {
        const filteredInfos = firstForInterleaveOnly ?
                              this.timestampInfos.filter((info: TimestampInfo): boolean => info.firstForInterleave) :
                              this.timestampInfos;
        const result: number[] =
            filteredInfos.map((info: TimestampInfo): number => info.endReceivedTimestamp - info.sentTimestamp);
        result.sort((a: number, b: number): number => a - b);
        return result;
    }

    // Settings.
    private readonly timestampInfoMaxAge = 120000; // 2 minutes.
    private readonly timestampAutoBufferMax = 0.995;
    private readonly timestampAutoBufferExtra = 180; // 3x maximum opus frame size.
    private readonly timestampAutoMinTarget = 500; // Minimum buffer target in ms to browsers that take time to seek.
    private readonly syncClockPeriod = 100;
    private readonly bufferExceedTickCounts = 3; // Number of buffer control ticks "grace period" for catch ups.
    private readonly catchUpEventDuration = 2000;

    // Constructor inputs.
    private readonly mediaElement: HTMLMediaElement;
    private readonly verbose: boolean;

    // Buffer mode.
    private autoBuffer: boolean = true;
    private maxBuffer: number = 0;

    // Network timing data.
    private timestampInfos: TimestampInfo[] = [];

    // Buffer control state machine.
    private bufferExceededCount: number = 0;

    // Tracking buffering performance.
    private catchUpEvents: number = 0;
    private catchUpEventsStart: number = 0;
    private lastCatchUpEventClusterEnd: number = 0;

    // Other internal stuff.
    private interval: number | null = null;

    // Debugging.
    private debugHandler: DebugHandler | null = null;
}
