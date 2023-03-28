import {TimestampInfo} from "./Deinterleave";
import {ReceivedInfo} from "./Stream";
import {BufferControlTickInfo, DebugHandler} from "./Debug";
import {API} from "live-video-streamer-common";

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
    constructor(mediaElement: HTMLMediaElement, verbose: boolean, onRecommendDowngrade: (() => void),
                debugHandler: DebugHandler | null = null) {
        this.mediaElement = mediaElement;
        this.onRecommendDowngrade = onRecommendDowngrade;
        this.verbose = verbose;
        this.setDebugHandler(debugHandler);
    }

    /**
     * Set buffer control parameters for a given video configuration from the server.
     *
     * @param params The parameters to set.
     */
    setBufferControlParameters(params: API.BufferControl): void {
        this.serverParams = params;
    }

    /**
     * Get the server parameters.
     */
    getServerParams(): API.BufferControl {
        return this.serverParams;
    }

    /**
     * Start the buffer control.
     */
    start(): void {
        const now = Date.now();
        this.catchUpEvents = 0;
        this.catchUpEventsStart = now;
        this.lastCatchUpEventClusterEnd = 0;
        this.waitingForInitialSeekSince = now;

        this.interval = window.setInterval((): void => {
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
     * @internal
     *
     * Set the handler to receive the performance and debugging information about the buffer.
     */
    setDebugHandler(debugHandler: DebugHandler | null): void {
        if (process.env["NODE_ENV"] === "development") {
            if (debugHandler !== null) {
                debugHandler.setBufferControl(this);
            }
            this.debugHandler = debugHandler;
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

        if (process.env["NODE_ENV"] === "development") {
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
    onRecieved(receivedInfo: ReceivedInfo): void {
        if (process.env["NODE_ENV"] === "development") {
            if (this.debugHandler !== null) {
                this.debugHandler.onReceived(receivedInfo);
            }
        }
    }

    /**
     * to be called whenever a new stream starts being received by the streamer.
     */
    onNewStreamStart(): void {
        const now = Date.now();

        // Now that we got a new stream (downgrade or otherwise), the buffering might be different.
        this.timestampInfos = [];
        this.catchUpEventsStart = now;
        this.lastCatchUpEventClusterEnd = 0;
        this.waitingForInitialSeekSince = now;

        // If we asked for a downgrade, we might have gotten it now, so stop waiting for it.
        this.waitingForNewStream = false;
    }

    /**
     * Get the length, in milliseconds, of the buffer in one of the media elements.
     *
     * @return The buffer length, in seconds, of the media element.
     */
    getBufferLength(): number {
        const buffered = this.mediaElement.buffered;
        if (buffered.length === 0 || this.mediaElement.currentTime < buffered.start(0)) {
            return NaN;
        }
        return (buffered.end(buffered.length - 1) - this.mediaElement.currentTime) * 1000;
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
        const tickInfo: BufferControlTickInfo = {
            timestamp: now,
            primaryBufferLength: bufferLength,
            catchUp: false,
            initialSeek: false
        };

        /* Figure out the target buffer size. */
        const delays: number[] = this.getSortedTimestampDelays();
        if (delays.length < 100) {
            this.maxBuffer = this.serverParams.initialBuffer;
        } else {
            this.maxBuffer = delays[Math.floor(delays.length * this.timestampAutoBufferMax)]! - delays[0]! +
                             this.serverParams.extraBuffer;
            this.maxBuffer = Math.max(this.maxBuffer,
                                      Math.max(this.timestampAutoMinTarget,
                                               this.serverParams.seekBuffer + this.serverParams.extraBuffer));
            this.maxBuffer = Math.max(this.maxBuffer, this.serverParams.minBuffer);
        }

        /* If the above algorithm is asking for this much buffer, then probably the network can't cope. */
        if (this.maxBuffer >= this.timestampAutoBufferDowngrade + this.serverParams.minBuffer &&
                              !this.waitingForNewStream)
        {
            this.waitingForNewStream = true;
            this.onRecommendDowngrade();
        }

        /* If we're waiting for the initial seek, but we're still downloading rapidly, then we haven't found the live
           edge yet. */
        const initialSeek = this.waitingForInitialSeekSince !== 0 &&
                            (now - this.waitingForInitialSeekSince) >= this.serverParams.minimumInitTime;
        if (this.waitingForInitialSeekSince !== 0 && !initialSeek) {
            this.debugBufferControlTick(tickInfo);
            return;
        }

        /* If we have sufficiently little buffer that we don't need to skip. */
        if (bufferLength <= this.maxBuffer && !initialSeek) {
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

        // We can't skip if there's no media at all. These are specified as normalized.
        const bufferRange = this.mediaElement.buffered;
        const seekRange = this.mediaElement.seekable;
        if (seekRange.length === 0 || bufferRange.length === 0) {
            if (this.verbose && !initialSeek) {
                console.warn("Cannot seek to catch up");
            }
            this.debugBufferControlTick(tickInfo);
            return;
        }

        // Figure out the last contiguous range that's both seekable and buffered.
        const rangeStart = Math.max(bufferRange.start(bufferRange.length - 1), seekRange.start(seekRange.length - 1));
        const rangeEnd = Math.min(bufferRange.end(bufferRange.length - 1), seekRange.end(seekRange.length - 1));

        // Don't skip ahead if there's not enough buffered contiguously
        const seekBufferSeconds = this.serverParams.seekBuffer / 1000;
        if (rangeStart > rangeEnd - seekBufferSeconds) {
            this.debugBufferControlTick(tickInfo);
            return;
        }

        // Seek.
        this.mediaElement.currentTime = rangeEnd - seekBufferSeconds;
        if (initialSeek) {
            this.waitingForInitialSeekSince = 0;
            tickInfo.initialSeek = true; // The initial seek that gets the stream to the live edge of the first segment.
        } else {
            this.catchUpEvents++;
            tickInfo.catchUp = true;
        }
        this.lastCatchUpEventClusterEnd = now + this.catchUpEventDuration;

        this.debugBufferControlTick(tickInfo);
    }

    private debugBufferControlTick(tickInfo: BufferControlTickInfo): void {
        if (process.env["NODE_ENV"] !== "development") {
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
    private serverParams: API.BufferControl = {
        minBuffer: 0,
        extraBuffer: 180, // 3x maximum opus frame size.
        initialBuffer: 1000,
        seekBuffer: 0,
        minimumInitTime: 500
    };

    private readonly timestampInfoMaxAge = 120000; // 2 minutes.
    private readonly timestampAutoBufferMax = 0.995;
    private readonly timestampAutoMinTarget = 500; // Minimum buffer target in ms to browsers that take time to seek.
    private readonly timestampAutoBufferDowngrade = 4000; // If data is this delayed, network is probably overloaded.
    private readonly syncClockPeriod = 100;
    private readonly bufferExceedTickCounts = 3; // Number of buffer control ticks "grace period" for catch ups.
    private readonly catchUpEventDuration = 2000;

    // Constructor inputs.
    private readonly mediaElement: HTMLMediaElement;
    private readonly onRecommendDowngrade: (() => void); // Called when a quality downgrade is recommended.
    private readonly verbose: boolean;

    // Buffer mode.
    private maxBuffer: number = 0;
    private waitingForNewStream: boolean = false;

    // Network timing data.
    private timestampInfos: TimestampInfo[] = [];

    // Buffer control state machine.
    private bufferExceededCount: number = 0;

    // Buffer control state machine for initial seek.
    private waitingForInitialSeekSince: number = 0;

    // Tracking buffering performance.
    private catchUpEvents: number = 0;
    private catchUpEventsStart: number = 0;
    private lastCatchUpEventClusterEnd: number = 0;

    // Other internal stuff.
    private interval: number | null = null;

    // Debugging.
    private debugHandler: DebugHandler | null = null;
}
