import {TimestampInfo} from "./Deinterleave";

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
     * @param primaryMediaElement The HTML media element to control the buffer level of.
     * @param secondaryMediaElements The secondary media elements to keep in sync with the primary media element.
     * @param verbose If true, some debug information is printed.
     */
    constructor(primaryMediaElement: HTMLMediaElement, secondaryMediaElements: HTMLMediaElement[],
                verbose: boolean) {
        this.primaryMediaElement = primaryMediaElement;
        this.secondaryMediaElements = secondaryMediaElements;
        this.verbose = verbose;

        for (let i = 0; i < secondaryMediaElements.length; i++) {
            this.secondaryMediaElementSync.push(0);
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
    }

    /**
     * Get the length, in milliseconds, of the buffer in one of the media elements.
     *
     * @param secondary The secondary media element index to get the buffer length for. If null (the default), the
     *                  primary media element's buffer length is returned.
     * @return The buffer length, in seconds, of the chosen media element.
     */
    getBufferLength(secondary: number | null = null): number {
        const mediaElement = (secondary === null) ? this.primaryMediaElement : this.secondaryMediaElements[secondary];
        const buffered = mediaElement!.buffered;
        if (buffered.length === 0) {
            return NaN;
        }
        return (buffered.end(buffered.length - 1) - this.primaryMediaElement.currentTime) * 1000;
    }

    /**
     * See how closely synchronized a secondary media element is to the primary media element.
     *
     * @param secondary The secondary element index to check.
     */
    getSecondarySync(secondary: number): number {
        return this.secondaryMediaElementSync[secondary]!;
    }

    // Automatically determine buffering.
    setAutoLatency(): void {
        this.autoBuffer = true;
        this.secondarySyncTolerance = 100;
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;
    }

    // Should get 3 seconds overall. Intended for the minimum quality to cope with poor connections/CDN buffering.
    setSaferLatency(): void {
        this.autoBuffer = false;
        this.maxBuffer = 1750;
        this.secondarySyncTolerance = 100;
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;
    }

    // Should get 2 seconds overall.
    setLowLatency(): void {
        this.autoBuffer = false;
        this.maxBuffer = 1000;
        this.secondarySyncTolerance = 100;
        this.catchUpEvents = 0;
        this.catchUpEventsStart = Date.now();
        this.lastCatchUpEventClusterEnd = 0;
    }

    // Should get 1 second overall.
    setUltraLowLatency(): void {
        this.autoBuffer = false;
        this.maxBuffer = 100;
        this.secondarySyncTolerance = 500; // It shouldn't, but audio needs more tolerance than video.
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

        /* When we first start playing, we need to seek to the end of the buffer. We also do this if the buffer gets
           so far out of sync the less aggressive sync operation below doesn't work (for some reason). */
        if (this.lastCatchUpEventClusterEnd <= now && bufferLength > this.skipThreshold) {
            const seekRange = this.primaryMediaElement.seekable;
            if (seekRange.length === 0) {
                return;
            }
            if (this.lastCatchUpEventClusterEnd !== 0) {
                if (this.verbose && process.env.NODE_ENV === "development") {
                    console.debug("Aggressive seek to catch up");
                }
                this.catchUpEvents++;
            }
            this.primaryMediaElement.currentTime = seekRange.end(seekRange.length - 1);
            this.lastCatchUpEventClusterEnd = Date.now().valueOf() + this.catchUpEventDuration;
            return;
        }

        /* Figure out the target buffer size. */
        if (this.autoBuffer) {
            const delays: number[] = this.getSortedTimestampDelays();
            if (delays.length < 100) {
                this.maxBuffer = 1000;
            } else {
                this.maxBuffer = delays[Math.floor(delays.length * this.timestampAutoBufferMax)]! - delays[0]! +
                                 this.timestampAutoBufferExtra;
            }
        }

        /* If we have sufficiently little buffer that we don't need to skip, then just ensure the secondary media
           element is in sync.*/
        if (bufferLength <= this.maxBuffer) {
            this.syncSecondaryMediaElements();
            return;
        }

        /* Give the seeking a chance to catch up if we've *just* done a seek. */
        if (this.lastCatchUpEventClusterEnd >= now) {
            this.syncSecondaryMediaElements();
            return;
        }

        /* Also, if we weren't already playing fast, then this is a new "buffering" event. */
        const seekRange = this.primaryMediaElement.seekable;
        if (seekRange.length === 0) {
            if (this.verbose) {
                console.warn("Cannot seek to catch up");
            }
            this.syncSecondaryMediaElements();
            return;
        }
        this.primaryMediaElement.currentTime = seekRange.end(seekRange.length - 1);
        this.lastCatchUpEventClusterEnd = now + this.catchUpEventDuration;
        this.catchUpEvents++;
    }

    /**
     * Synchronize the secondary media elements to the primary media element.
     */
    private syncSecondaryMediaElements(): void {
        /* If the video (the primary media element) is seeking, then there's not much point in trying to correct the
           secondary element locations. */
        if (this.primaryMediaElement.seeking) {
            return;
        }

        /* Correct each secondary element. */
        this.secondaryMediaElements.forEach((mediaElement: HTMLMediaElement, i: number): void => {
            // figure out how far out of sync they are.
            this.secondaryMediaElementSync[i] =
                (mediaElement.currentTime - this.primaryMediaElement.currentTime) * 1000;

            // If the secondary element is synchronized, then we don't need to do anything except match playback rates.
            if (Math.abs(this.secondaryMediaElementSync[i]!) <= this.secondarySyncTolerance) {
                mediaElement.playbackRate = this.primaryMediaElement.playbackRate;
                return;
            }

            // If the secondary element is seeking, then we need to give it a chance to finish doing so. This logic is
            // needed because sometimes seeking gets stuck.
            if (mediaElement.seeking && !this.seekStallMap.has(i)) {
                this.seekStallMap.set(i, 1); // We've already been seeking for one tick.
                return;
            }
            if (mediaElement.seeking && this.seekStallMap.get(i)! < this.secondarySeekTimeout) {
                this.seekStallMap.set(i, this.seekStallMap.get(i)! + 1);
                return;
            }
            this.seekStallMap.delete(i);

            // Figure out what adjustment to the playback rate would be needed to bring the secondary element in sync
            // with the primary element at the next tick.
            let adjustPlaybackRate: number =
                this.calculatePlaybackRate(mediaElement.currentTime * 1000, this.primaryMediaElement.currentTime * 1000,
                                           this.primaryMediaElement.playbackRate);

            // Adjust the playhead of the secondary media element to match that of the primary. Progress towards doing
            // so either by adjusting the playback rate (if possible and they're not too far out of sync), or by seeking
            // if necessary.
            if (!isNaN(adjustPlaybackRate) &&
                Math.abs(this.secondaryMediaElementSync[i]!) < this.secondarySkipThreshold) { // Adjust playback rate.
                adjustPlaybackRate = Math.min(Math.max(adjustPlaybackRate, this.minPlaybackRate), this.maxPlaybackRate);
                if (this.verbose && process.env.NODE_ENV === "development") {
                    console.debug(`Adjusting secondary playback rate ${i} to ${adjustPlaybackRate}`);
                }
                mediaElement.playbackRate = adjustPlaybackRate;
            } else { // Seek.
                if (this.verbose && process.env.NODE_ENV === "development") {
                    console.debug(`Try seeking secondary media element ${i} by ${-this.secondaryMediaElementSync[i]!}`);
                }

                // If there's nothing in the audio buffer, don't bother trying to seek. We'll try again later when the
                // buffer isn't empty.
                if (mediaElement.buffered.length === 0) {
                    if (this.verbose && process.env.NODE_ENV === "development") {
                        console.debug(`Buffer for secondary media element ${i} is empty`);
                    }
                    return;
                }

                // If the start of the audio buffer has progressed beyond the video, then we need to wait for the video
                // to catch up.
                if (mediaElement.buffered.start(0) > this.primaryMediaElement.currentTime) {
                    if (this.verbose && process.env.NODE_ENV === "development") {
                        console.debug(`Buffer for secondary media element ${i} is ahead of primary media element`);
                    }
                    return;
                }

                // Seek as close to the video's time as we can within the buffer.
                const bufferHead = mediaElement.buffered.end(mediaElement.buffered.length - 1);
                mediaElement.currentTime = Math.min(this.primaryMediaElement.currentTime, bufferHead);
            }
        });
    }

    /**
     * Calculates the playback rate needed to compensate for a given drift by the next tick.
     *
     * @param currentTime The current time of the clock to correct.
     * @param targetCurrentTime What would be the current time of the clock to correct if it didn't need correcting.
     * @param baseClockRate The rate at which the comparison clock ticks.
     */
    private calculatePlaybackRate(currentTime: number, targetCurrentTime: number, baseClockRate: number = 1): number {
        // The amount of time the base clock will progress by the next tick.
        const baseClockOffset = baseClockRate * this.syncClockPeriod;

        // The amount of time the target clock needs to progress to catch up with where it's supposed to be.
        const drift = targetCurrentTime - currentTime;

        // The total amount of time the target clock needs to progress by the next tick.
        const neededProgress = baseClockOffset + drift;

        // The actually needed playback rate.
        return neededProgress / this.syncClockPeriod;
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
    private readonly syncClockPeriod = 100;
    private readonly minPlaybackRate = 0.5;
    private readonly maxPlaybackRate = 2;
    private readonly skipThreshold = 4000;
    private readonly secondarySkipThreshold = 500;
    private readonly catchUpEventDuration = 2000;
    private readonly secondarySeekTimeout = 10; // If we were seeking for too long (in ticks), we probably got stuck.

    // Constructor inputs.
    private readonly primaryMediaElement: HTMLMediaElement;
    private readonly secondaryMediaElements: HTMLMediaElement[];
    private readonly verbose: boolean;

    // Buffer mode.
    private autoBuffer: boolean = true;
    private maxBuffer: number = 0;
    private secondarySyncTolerance: number = 0;

    // Network timing data.
    private timestampInfos: TimestampInfo[] = [];

    // Tracking buffering performance.
    private catchUpEvents: number = 0;
    private catchUpEventsStart: number = 0;
    private lastCatchUpEventClusterEnd: number = 0;

    // Tracking synchronization performance.
    private readonly secondaryMediaElementSync = new Array<number>();

    // Other internal stuff.
    private interval: number | null = null;
    private readonly seekStallMap = new Map<number, number>();
}
