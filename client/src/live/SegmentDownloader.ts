import {API, assertNonNull} from "live-video-streamer-common";
import {Deinterleaver, TimestampInfo} from "./Deinterleave";
import {Stream} from "../Stream";
import {assertType} from "@ckitching/typescript-is";

/**
 * Margin for error when calculating preavailability from segment info objects, in ms.
 *
 * 1s is the CDN cache time limit, and 500 ms is a tolerance.
 */
const PreavailabilityMargin = 1500;

/**
 * The period between download scheduler updates, measured in segments.
 */
const DownloadSchedulerUpdatePeriod = 16;

export interface ReceivedInfo {
    streamIndex: number,
    timestamp: number,
    length: number
}

/**
 * Schedules download of segments.
 */
export class SegmentDownloader {
    constructor(
        private readonly info: API.SegmentIndexDescriptor,
        private readonly interleaved: boolean,
        private readonly streams: Stream[],
        private readonly streamIndex: number,
        private readonly baseUrl: string,
        private readonly segmentDurationMS: number,
        private readonly segmentPreavailability: number,
        private readonly onTimestamp: (timestampInfo: TimestampInfo) => void,
        private readonly onReceived: (receivedInfo: ReceivedInfo) => void,
        private readonly onControlChunk: (data: ArrayBuffer, controlChunkType: number) => void,
        private readonly onError: (description: string) => void
    ) {
        this.setSegmentDownloadSchedule(info);
    }

    stop(): void {
        // Stop anything that's downloading.
        this.aborter.abort();

        // Stop the timers.
        this.segmentIndex = Infinity; // Stop the download of any new segments.
        if (this.downloadTimeout) {
            clearTimeout(this.downloadTimeout);
            this.downloadTimeout = null;
        }
        if (this.schedulerTimeout) {
            clearTimeout(this.schedulerTimeout);
            this.schedulerTimeout = null;
        }
    }

    /**
     * Sets the next segment index to download, and the time to start downloading the segment after.
     *
     * @param info A segment index info object that has just been downloaded.
     */
    private setSegmentDownloadSchedule(info: API.SegmentIndexDescriptor): void {
        /* Schedule periodic updates to the download scheduler. */
        const timeoutFn = async (): Promise<void> => {
            try {
                const response: Response = await fetch(`${this.baseUrl}/chunk-stream${this.streamIndex}-index.json`,
                    { signal: this.aborter.signal });
                if (response.status !== 200) {
                    throw new Error(`Fetching stream index descriptor gave HTTP status code ${response.status}`);
                }
                const newInfo: unknown = await response.json();
                assertType<API.SegmentIndexDescriptor>(newInfo);
                this.setSegmentDownloadSchedule(newInfo);
            } catch (ex: any) {
                const e = ex as Error;
                if (e.name === 'AbortError') {
                    return;
                }
                this.onError(`Error downloading stream index descriptor: ${e.message}`);
            }
        };
        this.schedulerTimeout = window.setTimeout((): void => {
            void timeoutFn(); // Cancellable via the combination of this.schedulerTimeout and this.aborter.
        }, this.segmentDurationMS * DownloadSchedulerUpdatePeriod);

        /* The currently available index. */
        let segmentIndex: number = info.index;

        /* When the next index becomes available for download. */
        let nextSegmentStartMS: number =
            this.segmentDurationMS - info.age - this.segmentPreavailability + PreavailabilityMargin;

        /* If the next segment is already preavailable, start with that one. */
        if (nextSegmentStartMS < 0) {
            segmentIndex++;
            nextSegmentStartMS += this.segmentDurationMS;
        }

        /* Never re-download a segment. This would be the case if we're already downloading the latest (pre)available
           segment. */
        if (segmentIndex < this.segmentIndex) {
            return;
        }

        /* The download() method expects the timestamp for the next segment to start, rather than an offset from now. */
        nextSegmentStartMS += Date.now();

        /* Cancel any scheduled download. */
        if (this.downloadTimeout) {
            clearTimeout(this.downloadTimeout);
            this.downloadTimeout = null;
        }

        /* Restart the download loop with the new parameters. */
        this.segmentIndex = segmentIndex;
        this.nextSegmentStart = nextSegmentStartMS;
        this.download();
    }

    private download(): void {
        /* Figure out the URL of the segment to download. */
        const segmentIndex = this.segmentIndex;
        let segmentIndexString: string = `${segmentIndex}`;
        while (segmentIndexString.length < this.info.indexWidth) {
            segmentIndexString = `0${segmentIndexString}`;
        }
        const url = this.interleaved ?
            `${this.baseUrl}/interleaved${this.streamIndex}-${segmentIndexString}` :
            `${this.baseUrl}/chunk-stream${this.streamIndex}-${segmentIndexString}.m4s`;

        /* Download the current segment. */
        void (async (): Promise<void> => {
            try {
                const response: Response = await fetch(url, { signal: this.aborter.signal });

                // Handle the error case.
                if (response.status !== 200) {
                    throw Error(`Returned HTTP status code ${response.status}`);
                }

                // If this is an old segment, just cancel it. A new one should be along soon if it hasn't already
                // arrived.
                if (segmentIndex < this.segmentIndex - 1) {
                    await response.body!.cancel();
                    return;
                }

                // Start reading from the response.
                await this.pump(response.body!.getReader(), `Stream ${this.streamIndex}, segment ${segmentIndex}`);
            } catch (ex: any) {
                const e = ex as Error;
                if (e.name === 'AbortError') {
                    return;
                }
                this.onError(`Error fetching chunk ${url}: ${e.message}`);
            }
        })();

        /* Schedule the downloading of the next segment. */
        this.segmentIndex++;
        this.downloadTimeout = window.setTimeout((): void => {
            this.download();
        }, this.nextSegmentStart - Date.now());
        this.nextSegmentStart += this.segmentDurationMS;
    }

    private async pump(reader: ReadableStreamDefaultReader, description: string): Promise<void> {
        try {
            // These are set by the first iteration of the loop (unless it early aborts). They're copied in the loop
            // because @typescript-eslint/no-loop-func can't tell that the capture is safe.
            let logicalSegmentIndex: number | null = null;
            let deinterleaver: Deinterleaver | null = null;

            while (true) {
                /* Try to get more data. */
                const readResult: ReadableStreamDefaultReadResult<Uint8Array> = await reader.read();
                const value: Uint8Array | undefined = readResult.value;
                const done: boolean = readResult.done;

                /* If we're done, then the value means nothing. */
                if (done) {
                    if (logicalSegmentIndex === null) {
                        return; // Empty segment!
                    }
                    for (const stream of this.streams) {
                        stream.endSegment(logicalSegmentIndex);
                    }
                    return;
                }
                assertNonNull(value);

                /* Notify the buffer control that we've received new data. */
                this.onReceived({
                    streamIndex: this.streamIndex,
                    timestamp: Date.now(),
                    length: value.length
                });

                /* Push the stream initialization before the segment. */
                if (logicalSegmentIndex === null) {
                    logicalSegmentIndex = this.logicalSegmentIndex;
                    this.logicalSegmentIndex++;

                    const logicalSegmentIndexCopy = logicalSegmentIndex;
                    this.streams.forEach((stream: Stream, index: number): void => {
                        stream.startSegment(logicalSegmentIndexCopy, `${description}, sub-stream ${index}`);
                    });
                }

                /* Handle the data we just downloaded. */
                if (this.interleaved) {
                    if (!deinterleaver) {
                        const logicalSegmentIndexCopy = logicalSegmentIndex;
                        deinterleaver = new Deinterleaver(
                            (data: ArrayBuffer, index: number): void => {
                                if (this.streams.length <= index) {
                                    return;
                                }
                                if (data.byteLength === 0) { // End of file.
                                    this.streams[index]!.endSegment(logicalSegmentIndexCopy);
                                    return;
                                }
                                this.streams[index]!.acceptSegmentData(data, logicalSegmentIndexCopy);
                            }, this.onControlChunk, (logicalSegmentIndex === 0) ? (): void => {} : this.onTimestamp,
                            description
                        );
                        // Note that the first segment is likely to be started in the middle, and therefore not good for
                        // network timing data.
                    }
                    deinterleaver.acceptData(value);
                } else {
                    this.streams[0]!.acceptSegmentData(value, logicalSegmentIndex);
                }
            }
        } catch (ex: any) {
            const e = ex as Error;
            if (e.name === 'AbortError') {
                return;
            }
            this.onError(`Error downloading or playing segment: ${e.message}`);
        }
    }

    private logicalSegmentIndex: number = 0;
    private segmentIndex: number = -1;
    private nextSegmentStart: number = 0;
    private downloadTimeout: number | null = null;
    private schedulerTimeout: number | null = null;
    private aborter: AbortController = new AbortController();
}
