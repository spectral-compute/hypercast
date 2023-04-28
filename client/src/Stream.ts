import * as Debug from "./Debug";

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
}

/**
 * Manages a media source buffer, and a queue of data to go with it.
 */
export class Stream {
    constructor(
        private readonly mediaSource: MediaSource,
        /** The duration of each segment in seconds */
        private readonly segmentDurationS: number,
        private readonly onError: (description: string) => void,
        private readonly onStart: (() => void) | null = null
    ) {
        if (process.env["NODE_ENV"] === "development") {
            this.checksum = new Debug.Addler32();
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

        this.segmentInitializations.set(segment, init);
    }

    /**
     * Notify the stream of the start of a new segment.
     *
     * @param segment The index of the segment. Must be successive integers, with the first being 0.
     * @param description A description of the segment.
     */
    startSegment(segment: number, description: string): void {
        if (this.isFinished()) {
            return;
        }

        if (segment === 0 && this.onStart) {
            this.onStart();
        }
        if (process.env["NODE_ENV"] === "development") {
            this.checksumDescriptions.set(segment, description);
        }
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
            if (this.sourceBuffer) {
                this.sourceBuffer.changeType(init.mimeType);
            } else {
                this.createSourceBuffer(init.mimeType);
            }

            // Append the initializer segment.
            this.sourceBuffer!.appendBuffer(init.init);
        }

        /* Now shovel as much data into the queue as we can. */
        while (this.queue.length > 0 && !this.sourceBuffer!.updating) {
            const data = this.queue.shift();
            if (data !== null) {
                this.sourceBuffer!.appendBuffer(data!);
                if (process.env["NODE_ENV"] === "development") {
                    this.checksum!.update(data!);
                }
            } else if (process.env["NODE_ENV"] === "development") {
                console.log(`[Media Source Buffer Checksum] ${this.checksumDescriptions.get(this.checksumIndex)!} ` +
                            `checksum: ${this.checksum!.getChecksum()}, length: ${this.checksum!.getLength()}`);
                this.checksum = new Debug.Addler32();
                this.checksumDescriptions.delete(this.checksumIndex);
                this.checksumIndex++;
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
        const end = buffered.end(buffered.length - 1);
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
     * require one from the start. So this can't happen until startSegment happens.
     */
    private createSourceBuffer(mimeType: string): void {
        this.sourceBuffer = this.mediaSource.addSourceBuffer(mimeType);
        this.sourceBuffer.addEventListener("onabort", (): void => {
            this.onError("SourceBuffer aborted");
        });
        this.sourceBuffer.addEventListener("onerror", (): void => {
            this.onError("Error with SourceBuffer");
        });
        this.sourceBuffer.addEventListener("update", (): void => {
            this.advanceQueue(); // We might have more data that we can add immediately.
        });
    }

    private sourceBuffer: SourceBuffer | null = null;
    private readonly queue = new Array<ArrayBuffer | null>();

    private readonly segmentInitializations = new Map<number, SegmentInitialization>();
    private readonly otherSegmentsQueue = new Map<number, ArrayBuffer[]>();
    private readonly finishedSegments = new Set<number>();
    private currentSegment: number = 0;

    private checksum: Debug.Addler32 | undefined;
    private checksumIndex: number = 0;
    private readonly checksumDescriptions = new Map<number, string>();
}
