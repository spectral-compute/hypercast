import {getFullMimeType, Stream} from "../Stream";
import {abortablePromise, assertNonNull} from "live-video-streamer-common";

/**
 * Information about the next segment.
 */
interface SegmentInfo {
    /**
     * The URL to download the segment from.
     */
    url: string;

    /**
     * How to initialize the decoder.
     */
    init: {
        /**
         * The MIME type of the segment.
         */
        mimeType: string;

        /**
         * The URL to download the initialization segment from.
         */
        url: string;
    } | null;

    /**
     * The index of thestream that got downloaded.
     */
    streamIndex: number;
}

export interface StreamInfo {
    mimeType: string;
    codecs: string;
    width?: number;
    height?: number;
    frameRate?: [number, number];
}

export class SegmentDownloaderItem {
    constructor(public readonly baseUrl: string, public readonly segmentCount: number,
                public readonly segmentDuration: number,
                public readonly streams: StreamInfo[], private readonly firstStream: number,
                private readonly chooseStream: (segmentDownloaderItem: SegmentDownloaderItem) => number) {}

    /**
     * Get the URL of a segment.
     */
    getSegmentInfo(segmentIndex: number): SegmentInfo {
        /* Choose a new stream. */
        let init = null;
        if (this.streamIndex === null) {
            this.streamIndex = this.chooseStream(this);
            init = {
                mimeType: getFullMimeType(this.streams[this.streamIndex]!.mimeType,
                                          this.streams[this.streamIndex]!.codecs),
                url: `${this.baseUrl}/init-stream${this.streamIndex! + this.firstStream}.m4s`
            };
        }

        /* Return information about the segment. */
        return {
            url: this.getSegmentUrl(segmentIndex),
            init: init,
            streamIndex: this.streamIndex! + this.firstStream
        };
    }

    /**
     * Unset the stream index so the selection process happens again when the next segment is requested.
     */
    clearStreamIndex(): void {
        this.streamIndex = null;
    }

    /**
     * Get the URL for a segment.
     */
    private getSegmentUrl(segmentIndex: number): string {
        let segmentIndexString: string = `${segmentIndex}`;
        while (segmentIndexString.length < 9) {
            segmentIndexString = `0${segmentIndexString}`;
        }
        return `${this.baseUrl}/chunk-stream${this.streamIndex! + this.firstStream}-${segmentIndexString}.m4s`;
    }

    /**
     * The index of the stream to download segments from.
     *
     * If set to null, the stream index either hasn't been chosen or it's been changed.
     */
    private streamIndex: number | null = null;
}

interface QueueItem {
    item: SegmentDownloaderItem;
    fulfill: (result: number) => void;
    reject: (e: Error) => void;
}

export class SegmentDownloader {
    constructor(private readonly stream: Stream,
                private readonly signal: AbortSignal,
                private waitToStartFirstSegment: (() => Promise<void>) | null,
                private readonly spawn: (fn: () => void | Promise<void>) => void,
                private readonly download: (url: string, type: "binary" | "json" | "string",
                                            what: string) => Promise<any>,
                private readonly onRecommendDowngrade: (downloader: SegmentDownloader,
                                                        item: SegmentDownloaderItem) => void) {
        /* We need a worker coroutine to do the shovelling. */
        this.spawn(async (): Promise<void> => {
            await this.worker();
        });
    }

    /**
     * Notify that we've reached the end of the downloads.
     */
    end(): void {
        this.queue.push(null);
        this.onNewQueueItem?.();
        this.onNewQueueItem = null;
    }

    /**
     * Append an item to the queue of items to stream.
     *
     * The playback will start once the other queued items finish playing.
     */
    append(item: SegmentDownloaderItem): Promise<number> {
        /* Create a promise that fulfills with the PTS of the start of the item. */
        const promise = abortablePromise<number>((fulfill: (result: number) => void, reject: (e: Error) => void) => {
            this.queue.push({item, fulfill, reject});
        }, this.signal);

        /* Notify the queue handler that there's a new item. */
        this.onNewQueueItem?.();
        this.onNewQueueItem = null;

        /* Done :) */
        return promise;
    }

    /**
     * Clear the stream index of the item that's currently streaming.
     *
     * This makes it re-run the stream choosing algorithm, e.g: after an ABR downgrade. The other items shouldn't have
     * an index chosen yet.
     */
    clearStreamIndex(): void {
        this.queue[0]?.item.clearStreamIndex();
    }

    private async worker(): Promise<void> {
        let streamSegmentIndex: number = 0;

        for (let itemIndex: number = 0; ; itemIndex++) {
            /* Wait for there to be items in the queue. */
            while (this.queue.length === 0) {
                await abortablePromise<void>((fulfill: () => void) => {
                    this.onNewQueueItem = fulfill;
                }, this.signal);
            }

            /* Get the first item. A null item means end of queue. */
            // Leave the item in the queue so clearStreamIndex works.
            const item: QueueItem | null = this.queue[0] as (QueueItem | null); // Not undefined.
            if (item === null) {
                return;
            }

            /* Download the segments into the stream. */
            for (let itemSegmentIndex = 1; itemSegmentIndex <= item.item.segmentCount; itemSegmentIndex++) {
                // Wait for space in the conceptual buffer so we don't download everything faster than it can be played
                // and then have to store it all in memory.
                // TODO

                // Get the next segment's downloading specification.
                const segmentInfo: SegmentInfo = item.item.getSegmentInfo(itemSegmentIndex);

                // Update the segment type. This should always run on the first iteration.
                if (segmentInfo.init !== null) {
                    this.stream.startSegmentSequence({
                        mimeType: segmentInfo.init.mimeType,
                        init: await this.download(segmentInfo.init.url, "binary", "initializer segment"),
                        segmentDuration: item.item.segmentDuration
                    }, streamSegmentIndex);
                }

                // Wait for it to be time to start the first segment. This is used by MseWrapper to make sure all the
                // source buffers are added first (via Stream.startSegmentSequence).
                if (this.waitToStartFirstSegment) {
                    await this.waitToStartFirstSegment();
                    this.waitToStartFirstSegment = null;
                }

                // We're starting a new segment.
                const promise =  this.stream.startSegment(streamSegmentIndex,
                                                          `item ${itemIndex}, stream ${segmentInfo.streamIndex}, ` +
                                                          `segment ${itemSegmentIndex}`);

                // Set up the resolution of the promise from append for this item.
                if (itemSegmentIndex === 1) {
                    this.spawn(((promise, fulfill) => async (): Promise<void> => {
                        fulfill(await promise);
                    })(promise, item.fulfill));
                }

                // Download the segment into the stream.
                const downloadTime: number = await this.streamSegment(segmentInfo.url, streamSegmentIndex);

                // Figure out if we should recommend a downgrade.
                if (downloadTime > item.item.segmentDuration) {
                    // Tell the user of this class about it.
                    this.onRecommendDowngrade(this, item.item);
                }

                // We've finished donwloading the segment.
                this.stream.endSegment(streamSegmentIndex);
                streamSegmentIndex++;
            }

            /* We're done with the first queue item. */
            this.queue.shift();
        }
    }

    /**
     * Download a segment in a streaming fashion, and time how long this takes.
     *
     * @param url The URL to download.
     * @param segmentIndex The index in this.stream to download to.
     * @return The time, in ms, that the donwload took.
     */
    private async streamSegment(url: string, segmentIndex: number): Promise<number> {
        // TODO: Throttle the download to be a bit faster (for safety, and so the ABR downgrade works) than real time so
        //       we don't overload the connection while streaming live in parallel.
        try {
            /* Start timing how long the download takes. */
            const start: number = performance.now();

            /* Send the request and check it's OK. */
            const response: Response = await fetch(url, { signal: this.signal });
            if (response.status !== 200) {
                throw new Error(`got HTTP status code ${response.status}`);
            }

            /* Incrementally download. */
            const reader: ReadableStreamDefaultReader = response.body!.getReader();
            while (true) {
                // Get the next chunk.
                const readResult: ReadableStreamDefaultReadResult<Uint8Array> = await reader.read();
                const value: Uint8Array | undefined = readResult.value;
                const done: boolean = readResult.done;

                // See if we're done.
                if (done) {
                    break;
                }
                assertNonNull(value);

                // Give the data to the stream.
                this.stream.acceptSegmentData(value, segmentIndex);
            }

            /* Figure out how long that took. */
            return performance.now() - start;
        } catch (ex: any) {
            const e = ex as Error;
            if (e.name === 'AbortError') {
                throw e;
            }
            throw Error(`Error downloading interjection segment: ${e.message}.`);
        }
    }

    /**
     * Queue of items that haven't started downloading.
     */
    private readonly queue: Array<QueueItem | null> = new Array();

    /**
     * Notify when we've got a new queue item.
     */
    private onNewQueueItem: (() => void) | null = null;
}
