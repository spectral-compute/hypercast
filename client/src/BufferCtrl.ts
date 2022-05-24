class Ewma {
    constructor(halfLife: number) {
        this.halfLife = halfLife;
    }

    reset(): void {
        this.value = 0;
    }

    get(): number {
        return this.getForTime(Date.now().valueOf());
    }

    add(value: number): number {
        const time = Date.now().valueOf();
        this.value = this.getForTime(time) + value;
        this.lastUpdate = time;
        return this.value;
    }

    private getForTime(time: number): number {
        const halfLifes = (time - this.lastUpdate) / this.halfLife;
        return this.value * 2 ** -halfLifes;
    }

    private value: number = 0;
    private lastUpdate: number = 0;
    private readonly halfLife: number;
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
        this.catchUpEventsEwma.reset();
        this.lastCatchUpEventClusterEnd = Date.now().valueOf() + this.catchUpInitDuration;

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

    // Should get 3 seconds overall. Intended for the minimum quality to cope with poor connections/CDN buffering.
    setSaferLatency(): void {
        this.minBuffer = 750;
        this.maxBuffer = 1750;
        this.secondarySyncTolerance = 100;
        this.catchUpEventsEwma.reset();
        this.lastCatchUpEventClusterEnd = Date.now().valueOf() + this.catchUpInitDuration;
    }

    // Should get 2 seconds overall.
    setLowLatency(): void {
        this.minBuffer = 500;
        this.maxBuffer = 1000;
        this.secondarySyncTolerance = 100;
        this.catchUpEventsEwma.reset();
        this.lastCatchUpEventClusterEnd = Date.now().valueOf() + this.catchUpInitDuration;
    }

    // Should get 1 second overall.
    setUltraLowLatency(): void {
        this.minBuffer = 50;
        this.maxBuffer = 100;
        this.secondarySyncTolerance = 500; // It shouldn't, but audio needs more tolerance than video.
        this.catchUpEventsEwma.reset();
        this.lastCatchUpEventClusterEnd = Date.now().valueOf() + this.catchUpInitDuration;
    }

    /**
     * Get the current buffer targets.
     */
    getBufferTargets(): [number, number] {
        return [this.minBuffer, this.maxBuffer];
    }

    /**
     * Get the catch up event exponentially weighted moving average.
     */
    getCatchUpEventEwma(): number {
        return this.catchUpEventsEwma.get();
    }

    private bufferControlTick(): void {
        const currentRate = this.primaryMediaElement.playbackRate;
        const bufferLength = this.getBufferLength();

        /* If we've fallen too far behind, or if there's a gap we need to seek over, use a seek to get to the end of the
           buffer. */
        if (bufferLength > this.skipThreshold || this.primaryMediaElement.buffered.length > 1) {
            const seekRange = this.primaryMediaElement.seekable;
            if (seekRange.length === 0) {
                if (this.verbose) {
                    console.warn("Cannot seek to catch up");
                }
                this.syncSecondaryMediaElements();
                return;
            }

            this.primaryMediaElement.currentTime = seekRange.end(seekRange.length - 1);
            if (this.verbose) {
                console.log("Seek to catch up");
            }
            this.syncSecondaryMediaElements();
            return;
        }

        /* If we have less buffer than the minimum set, then no fast play. */
        if (bufferLength <= this.minBuffer && currentRate > 1) {
            this.primaryMediaElement.playbackRate = 1;
            this.syncSecondaryMediaElements();
            return;
        }

        /* If we're between the minimum length and the target length, use hysteresis: keep playing at a normal rate if
           we were already doing so. If not, then the catch-up logic in the next step will apply. */
        if (bufferLength <= this.maxBuffer && currentRate === 1) {
            this.syncSecondaryMediaElements();
            return;
        }

        /* Otherwise, apply a sped-up rate. */
        this.primaryMediaElement.playbackRate = this.maxPlaybackRate;

        /* Also, if we weren't already playing fast, then this is a new "buffering" event. */
        const now = Date.now().valueOf();
        if (currentRate === 1 && this.lastCatchUpEventClusterEnd < now) {
            this.lastCatchUpEventClusterEnd = now + this.catchUpEventDuration;
            this.catchUpEventsEwma.add(1);
        }
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
                if (this.verbose) {
                    console.log(`Adjusting secondary playback rate ${i} to ${adjustPlaybackRate}`);
                }
                mediaElement.playbackRate = adjustPlaybackRate;
            } else { // Seek.
                if (this.verbose) {
                    console.log(`Try seeking secondary media element ${i} by ${-this.secondaryMediaElementSync[i]!}`);
                }

                // If there's nothing in the audio buffer, don't bother trying to seek. We'll try again later when the
                // buffer isn't empty.
                if (mediaElement.buffered.length === 0) {
                    if (this.verbose) {
                        console.log(`Buffer for secondary media element ${i} is empty`);
                    }
                    return;
                }

                // If the start of the audio buffer has progressed beyond the video, then we need to wait for the video
                // to catch up.
                if (mediaElement.buffered.start(0) > this.primaryMediaElement.currentTime) {
                    if (this.verbose) {
                        console.log(`Buffer for secondary media element ${i} is ahead of primary media element`);
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

    // Settings.
    private readonly syncClockPeriod = 100;
    private readonly minPlaybackRate = 0.5;
    private readonly maxPlaybackRate = 2;
    private readonly skipThreshold = 4000;
    private readonly secondarySkipThreshold = 500;
    private readonly catchUpEventDuration = 2000;
    private readonly catchUpInitDuration = 10000;
    private readonly secondarySeekTimeout = 10; // If we were seeking for too long (in ticks), we probably got stuck.

    // Constructor inputs.
    private readonly primaryMediaElement: HTMLMediaElement;
    private readonly secondaryMediaElements: HTMLMediaElement[];
    private readonly verbose: boolean;

    // Buffer mode.
    private minBuffer: number = 0;
    private maxBuffer: number = 0;
    private secondarySyncTolerance: number = 0;

    // Tracking buffering performance.
    private readonly catchUpEventsEwma: Ewma = new Ewma(60000);
    private lastCatchUpEventClusterEnd: number = 0;

    // Tracking synchronization performance.
    private readonly secondaryMediaElementSync = new Array<number>();

    // Other internal stuff.
    private interval: number | null = null;
    private readonly seekStallMap = new Map<number, number>();
}