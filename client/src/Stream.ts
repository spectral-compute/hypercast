import * as Debug from "./Debug";
import {EventDispatcher} from "./EventDispatcher";
import {PlayerErrorEvent} from "./Player";

/**
 * Describe how to switch to a new sequence of segments.
 */
interface SegmentInitialization {
    /**
     * The MIME type of the sequence of segments, including the codecs parameter.
     */
    mimeType: string;

    /**
     * The initializer segment.
     */
    init: ArrayBuffer;

    /**
     * The duration of the segments in ms.
     */
    segmentDuration: number;
}

export class StreamStartEvent extends Event {
    constructor() {
        super("start");
    }
}

export interface StreamEventMap {
    "start": StreamStartEvent,
    "error": PlayerErrorEvent
}

/**
 * Manages a media source buffer, and a queue of data to go with it.
 */
export class Stream extends EventDispatcher<keyof StreamEventMap, StreamEventMap> {
    constructor(
        private readonly mediaSource: MediaSource,
        private readonly sequential: boolean = false,
        private readonly mediaElement: HTMLMediaElement | null = null ///< Don't prune after this video's current time.
    ) {
        super();

        if (process.env["NODE_ENV"] === "development") {
            this.checksum = new Debug.Adler32();
        }
    }

    /**
     * Notify that a new sequence of segments is going to start.
     *
     * This must be called before the call to startSegment for the same segment index.
     *
     * @param segment The index of the segment.
     */
    startSegmentSequence(init: SegmentInitialization, segment: number): void {
        if (this.isFinished()) {
            return;
        }

        if (!this.sourceBuffer) {
            this.createSourceBuffer(init.mimeType);
        }
        this.segmentInitializations.set(segment, init);
    }

    /**
     * Notify the stream of the start of a new segment.
     *
     * @param segment The index of the segment. Must be successive integers, with the first being 0.
     * @param description A description of the segment.
     * @return A promise that resolves when the segment is enqueued with the PTS of the start of the segment.
     */
    startSegment(segment: number, description: string): Promise<number> {
        if (this.isFinished()) {
            return new Promise((_, reject) => reject());
        }

        if (segment === 0) {
            this.dispatchEvent(new StreamStartEvent());
        }

        if (process.env["NODE_ENV"] === "development") {
            this.checksumDescriptions.set(segment, description);
        }

        return new Promise((resolve, reject) => {
            if (segment === 0) {
                resolve(0);
            } else if (this.segmentTimestampResults.has(segment!)) {
                const result: number = this.segmentTimestampResults.get(segment!)!;
                this.segmentTimestampResults.delete(segment!);
                resolve(result);
            } else {
                this.segmentTimestampPromiseResolvers.set(segment!, [resolve, reject]);
            }
        })
    }

    /**
     * Notify that there will be no more data for a given segment.
     *
     * It is legal to call this more than once for the same segment.
     *
     * @param segment The segment that is now finished.
     */
    endSegment(segment: number): void {
        if (this.isFinished()) {
            return;
        }

        /* Make this idempotent. */
        if (segment < this.currentSegment) {
            return;
        }

        /* Newly finished segment. */
        this.finishedSegments.add(segment);

        /* Add as much to the queue as possible. */
        while (this.finishedSegments.has(this.currentSegment)) {
            // Add data we collected for the segment to the queue.
            this.moveOtherSegmentBufferToQueue();

            // Let advanceQueue() know about the end of the segment.
            this.queue.push(null);

            // We're completely done with this segment now, and don't ened to track that it's completed.
            this.finishedSegments.delete(this.currentSegment);

            // Having moved the whole of this segment, we're ready to start the next.
            this.currentSegment++;
        }

        /* Add any data we have for the new, unfinished, segment to the queue. */
        this.moveOtherSegmentBufferToQueue();

        /* Start passing the newly queued items to the MSE buffer. */
        this.advanceQueue();
    }

    /**
     * Add data for a given segment.
     *
     * @param data The data to enqueue.
     * @param segment The segment that the data is from.
     */
    acceptSegmentData(data: ArrayBuffer, segment: number): void {
        if (this.isFinished()) {
            return;
        }

        if (segment === this.currentSegment) {
            this.queue.push(data);
            this.advanceQueue();
        } else {
            if (!this.otherSegmentsQueue.has(segment)) {
                this.otherSegmentsQueue.set(segment, new Array<ArrayBuffer>());
            }
            this.otherSegmentsQueue.get(segment)!.push(data);
        }
    }

    /**
     * No more data is to be passed into the media source from this stream.
     */
    end(): void {
        if (!this.sourceBuffer) {
            return;
        }
        this.mediaSource.removeSourceBuffer(this.sourceBuffer);

        /* Reject any promises we have. */
        for (const [_segment, [_resolve, reject]] of this.segmentTimestampPromiseResolvers) {
            reject();
        }
    }

    /**
     * Try to add more data to the MSE buffer.
     */
    private advanceQueue(): void {
        if (this.isFinished()) {
            return;
        }

        /* We can only advance the queue at all if we're not still updating. */
        if (this.sourceBuffer?.updating) {
            return;
        }

        /* Start by pruning. */
        this.prune();
        if (this.sourceBuffer?.updating) {
            return;
        }

        /* Initialize for a new segment type if necessary. */
        // The body of this if statement assumes this isn't happening mid-segment.
        if (this.segmentInitializations.has(this.currentSegment)) {
            // Get and remove the initialization information from the map.
            const init: SegmentInitialization = this.segmentInitializations.get(this.currentSegment)!;
            this.segmentInitializations.delete(this.currentSegment);

            // Set the source buffer mimetype. This might create the source buffer, since we need the first mimetype to
            // do that.
            this.sourceBuffer!.changeType(init.mimeType);

            // Append the initializer segment.
            this.sourceBuffer!.appendBuffer(init.init);

            // Print debug information about the initializer segment.
            if (process.env["NODE_ENV"] === "development") {
                const checksum = new Debug.Adler32();
                checksum.update(init.init);
                console.log("[Media Source Buffer Checksum] " + this.checksumDescriptions.get(this.currentSegment)! +
                            ` initializer checksum: ${checksum.getChecksum()}, length: ${checksum.getLength()}`);
            }

            // Handle the change in segment duration. This is the simplest, though not most efficient, way to guarantee
            // we never prune a segment that's still in use.
            // TODO: Proper handover to shorter durations.
            this.segmentDurationS = Math.max(init.segmentDuration / 1000, this.segmentDurationS);
        }

        /* Now shovel as much data into the queue as we can. */
        while (this.queue.length > 0 && !this.sourceBuffer!.updating) {
            const data = this.queue.shift();

            // Not the end of the segment.
            if (data !== null) {
                // Actually append the data.
                this.sourceBuffer!.appendBuffer(data!);
                if (process.env["NODE_ENV"] === "development") {
                    this.checksum!.update(data!);
                }
                return;
            }

            // End of segment.

            // Print debug information about the segment.
            if (process.env["NODE_ENV"] === "development") {
                console.log("[Media Source Buffer Checksum] " +
                            this.checksumDescriptions.get(this.completelyBufferedSegments)! +
                            ` checksum: ${this.checksum!.getChecksum()}, length: ${this.checksum!.getLength()}`);
                this.checksum = new Debug.Adler32();
                this.checksumDescriptions.delete(this.completelyBufferedSegments);
            }

            // We've finished this segment.
            this.completelyBufferedSegments++;

            // Resolve the segment timestamp, or save the timestamp for immediate resolution when that segment is
            // created.
            const buffered = this.sourceBuffer!.buffered;
            const timestamp: number = buffered.end(buffered.length - 1);

            if (this.segmentTimestampPromiseResolvers.has(this.completelyBufferedSegments)) {
                const [resolve, _] = this.segmentTimestampPromiseResolvers.get(this.completelyBufferedSegments)!;
                this.segmentTimestampPromiseResolvers.delete(this.completelyBufferedSegments);
                resolve(timestamp);
            } else {
                this.segmentTimestampResults.set(this.completelyBufferedSegments, timestamp);
            }
        }
    }

    /**
     * Remove stale data from the MSE buffer.
     */
    private prune(): void {
        if (this.isFinished() || !this.sourceBuffer) {
            return;
        }

        /* Figure out if anything is buffered at all. */
        const buffered = this.sourceBuffer.buffered;
        if (buffered.length === 0) {
            return;
        }

        /* Figure out if we have so much buffered that we're going to do a prune operation. */
        const start = buffered.start(0);
        const end = Math.min(buffered.end(buffered.length - 1), this.mediaElement?.currentTime ?? -Infinity);
        if (end - this.segmentDurationS * 3 <= start) {
            return;
        }

        /* Prune :) */
        this.sourceBuffer.remove(start, end - this.segmentDurationS * 2);
    }

    /**
     * Move data from the other segment buffer for the current segment to the current segment queue.
     */
    private moveOtherSegmentBufferToQueue(): void {
        if (!this.otherSegmentsQueue.has(this.currentSegment)) {
            return;
        }
        for (const buffer of this.otherSegmentsQueue.get(this.currentSegment)!) {
            this.queue.push(buffer);
        }
        this.otherSegmentsQueue.delete(this.currentSegment);
    }

    /**
     * Check that the media source isn't finished with.
     *
     * The media source can be externally disconnected from the underlying media element, such as when the stream is
     * changed during playback. This method detects conditions like that so that we don't continue trying to add data to
     * such a media source.
     */
    private isFinished(): boolean {
        return this.mediaSource.readyState !== "open";
    }

    /**
     * Create the sourceBuffer object.
     *
     * This would be in the constructor, but even though source buffers allow their mime type to be changed, they still
     * require one from the start. So this can't happen until startSegmentSequence happens.
     */
    private createSourceBuffer(mimeType: string): void {
        this.sourceBuffer = this.mediaSource.addSourceBuffer(mimeType);
        if (this.sequential) {
            this.sourceBuffer.mode = "sequence";
        }
        this.sourceBuffer.addEventListener("onabort", (): void => {
            this.dispatchEvent(new PlayerErrorEvent(new Error("SourceBuffer aborted")));
        });
        this.sourceBuffer.addEventListener("onerror", (): void => {
            this.dispatchEvent(new PlayerErrorEvent(new Error("SourceBuffer aborted")));
        });
        this.sourceBuffer.addEventListener("update", (): void => {
            this.advanceQueue(); // We might have more data that we can add immediately.
        });
    }

    private sourceBuffer: SourceBuffer | null = null;
    private readonly queue = new Array<ArrayBuffer | null>();

    private readonly segmentInitializations = new Map<number, SegmentInitialization>();
    private readonly otherSegmentsQueue = new Map<number, ArrayBuffer[]>();
    private readonly segmentTimestampPromiseResolvers = new Map<number, [any, any]>();
    private readonly segmentTimestampResults = new Map<number, number>();
    private readonly finishedSegments = new Set<number>();
    private currentSegment: number = 0;
    private completelyBufferedSegments: number = 0;

    /**
     * The duration of each new segment in seconds.
     */
    private segmentDurationS: number = 0;

    private checksum: Debug.Adler32 | undefined;
    private readonly checksumDescriptions = new Map<number, string>();
}

/**
 * Get a full MIME type (that can be given to the other APIs) from the generic MIME type and codecs string.
 */
export function getFullMimeType(mimeType: string, codecs: string) {
    return `${mimeType}; codecs="${codecs}"`
}
