import * as deinterleave from './deinterleave.js';

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

/**
 * Length of media source buffer to keep after pruning.
 */
const PruneMinMediaSourceBufferLength = 8000;

/**
 * Threshold of media source buffer to trigger pruning.
 */
const PruneMaxMediaSourceBufferLength = 16000;

/**
 * Manages a media source buffer, and a queue of data to go with it.
 */
class Stream {
    constructor(mediaSource: MediaSource, init: ArrayBuffer, mimeType: string, onStart: (() => void) | null = null) {
        this.mediaSource = mediaSource;
        this.init = init;
        this.onStart = onStart;

        this.sourceBuffer = this.mediaSource.addSourceBuffer(mimeType);
        this.sourceBuffer.addEventListener('update', (): void => {
            this.advanceQueue(); // We might have more data that we can add immediately.
        });
    }

    /**
     * Notify the stream of the start of a new segment.
     *
     * @param segment The index of the segment. Must be successive integers, with the first being 0.
     */
    startSegment(segment: number): void {
        if (segment == 0 && this.onStart) {
            this.onStart();
        }
        this.acceptSegmentData(this.init, segment);
    }

    /**
     * Notify that there will be no more data for a given segment.
     *
     * It is legal to call this more than once for the same segment.
     *
     * @param segment The segment that is now finished.
     */
    endSegment(segment: number): void {
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
        if (segment == this.currentSegment) {
            this.queue.push(data);
            this.advanceQueue();
        }
        else {
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
        this.mediaSource.removeSourceBuffer(this.sourceBuffer);
    }

    /**
     * Try to add more data to the MSE buffer.
     */
    private advanceQueue(): void {
        /* We can only advance the queue at all if we're not still updating. */
        if (this.sourceBuffer.updating) {
            return;
        }

        /* Start by pruning. */
        this.prune();

        /* Now shovel as much data into the queue as we can. */
        while (this.queue.length > 0 && !this.sourceBuffer.updating) {
            this.sourceBuffer.appendBuffer(this.queue.shift()!);
        }
    }

    /**
     * Remove stale data from the MSE buffer.
     */
    private prune(): void {
        /* Figure out if anything is buffered at all. */
        const buffered = this.sourceBuffer.buffered;
        if (buffered.length == 0) {
            return;
        }

        /* Figure out if we have so much buffered that we're going to do a prune operation. */
        const start = buffered.start(0);
        const end = buffered.end(buffered.length - 1);
        if (end - PruneMaxMediaSourceBufferLength / 1000 <= start) {
            return;
        }

        /* Prune :) */
        this.sourceBuffer.remove(start, end - PruneMinMediaSourceBufferLength / 1000);
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

    private readonly mediaSource: MediaSource;
    private readonly init: ArrayBuffer;
    private readonly onStart: (() => void) | null;

    private readonly sourceBuffer: SourceBuffer;
    private readonly queue = new Array<ArrayBuffer>();

    private readonly otherSegmentsQueue = new Map<number, Array<ArrayBuffer>>();
    private readonly finishedSegments = new Set<number>();
    private currentSegment: number = 0;
}

/**
 * Schedules download of segments.
 */
class SegmentDownloader {
    constructor(info: any, interleaved: boolean, streams: Array<Stream>, streamIndex: number,
                baseUrl: string, segmentDuration: number, segmentPreavailability: number) {
        this.info = info;
        this.interleaved = interleaved;
        this.streams = streams;
        this.streamIndex = streamIndex;
        this.baseUrl = baseUrl;
        this.segmentDuration = segmentDuration;
        this.segmentPreavailability = segmentPreavailability;

        this.setSegmentDownloadSchedule(info);
    }

    stop(): void {
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
     * @param A segment index info object that has just been downloaded.
     */
    private setSegmentDownloadSchedule(info: any): void {
        /* Schedule periodic updates to the download scheduler. */
        this.schedulerTimeout = setTimeout((): void => {
            fetch(`${this.baseUrl}/chunk-stream${this.streamIndex}-index.json`).
            then((response: Response): Promise<any> =>
            {
                return response.json();
            }).then((newInfo): void => {
                this.setSegmentDownloadSchedule(newInfo);
            });
        }, this.segmentDuration * DownloadSchedulerUpdatePeriod);

        /* The currently available index. */
        let segmentIndex: number = info.index;

        /* When the next index becomes available for download. */
        let nextSegmentStart: number =
            this.segmentDuration - info.age - this.segmentPreavailability + PreavailabilityMargin;

        /* If the next segment is already preavailable, start with that one. */
        if (nextSegmentStart < 0) {
            segmentIndex++;
            nextSegmentStart += this.segmentDuration;
        }

        /* Never re-download a segment. This would be the case if we're already downloading the latest (pre)available
           segment. */
        if (segmentIndex < this.segmentIndex) {
            return;
        }

        /* The download() method expects the timestamp for the next segment to start, rather than an offset from now. */
        nextSegmentStart += Date.now();

        /* Cancel any scheduled download. */
        if (this.downloadTimeout) {
            clearTimeout(this.downloadTimeout);
            this.downloadTimeout = null;
        }

        /* Restart the download loop with the new parameters. */
        this.segmentIndex = segmentIndex;
        this.nextSegmentStart = nextSegmentStart;
        this.download();
    }

    private download(): void {
        /* Figure out the URL of the segment to download. */
        let segmentIndexString: string = '' + this.segmentIndex;
        while (segmentIndexString.length < this.info.indexWidth) {
            segmentIndexString = '0' + segmentIndexString;
        }
        const url = this.interleaved ?
                    `${this.baseUrl}/interleaved${this.streamIndex}-${segmentIndexString}` :
                    `${this.baseUrl}/chunk-stream${this.streamIndex}-${segmentIndexString}.m4s`;

        /* Download the current segment. */
        fetch(url).then((response: Response): void => {
            this.pump(response.body!.getReader()); // TODO: Error handling.
        });

        /* Schedule the downloading of the next segment. */
        this.segmentIndex++;
        this.downloadTimeout = setTimeout((): void => {
            this.download();
        }, this.nextSegmentStart - Date.now());
        this.nextSegmentStart += this.segmentDuration;
    }

    private pump(reader: ReadableStreamDefaultReader, logicalSegmentIndex: number | null = null,
                 deinterleaver: deinterleave.Deinterleaver | null = null): void {
        reader.read().then(({done, value}): void => {
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

            /* Push the stream initialization before the segment. */
            if (logicalSegmentIndex === null) {
                logicalSegmentIndex = this.logicalSegmentIndex;
                this.logicalSegmentIndex++;
                for (const stream of this.streams) {
                    stream.startSegment(logicalSegmentIndex);
                }
            }

            /* Handle the data we just downloaded. */
            if (this.interleaved) {
                if (!deinterleaver) {
                    deinterleaver = new deinterleave.Deinterleaver((data: ArrayBuffer, index: number): void => {
                        if (this.streams.length <= index) {
                            return;
                        }
                        if (data.byteLength == 0) { // End of file.
                            this.streams[index]!.endSegment(logicalSegmentIndex!);
                            return;
                        }
                        this.streams[index]!.acceptSegmentData(data, logicalSegmentIndex!);
                    });
                }
                deinterleaver.acceptData(value);
            }
            else {
                this.streams[0]!.acceptSegmentData(value, logicalSegmentIndex);
            }

            /* This is essentially a loop. */
            this.pump(reader, logicalSegmentIndex, deinterleaver);
        });
    }

    private readonly info;
    private readonly interleaved: boolean;
    private readonly streams: Array<Stream>;
    private readonly streamIndex: number;
    private readonly baseUrl: string;
    private readonly segmentDuration: number;
    private readonly segmentPreavailability: number;

    private logicalSegmentIndex: number = 0;
    private segmentIndex: number = -1;
    private nextSegmentStart: number = 0;
    private downloadTimeout: number | null = null;
    private schedulerTimeout: number | null = null;
}

export class MseWrapper {
    constructor(video: HTMLVideoElement, audio: HTMLAudioElement, segmentDuration: number, segmentPreavailability: number) {
        this.video = video;
        this.audio = audio;
        this.segmentDuration = segmentDuration;
        this.segmentPreavailability = segmentPreavailability;
    }

    setSource(baseUrl: string, videoStream: number, audioStream: number, interleaved: boolean): void {
        /* If we're setting the source to what it's already set to, do nothing. */
        if (baseUrl === this.baseUrl && videoStream === this.videoStreamIndex &&
            audioStream === this.audioStreamIndex)
        {
            return;
        }
        this.baseUrl = baseUrl;
        this.videoStreamIndex = videoStream;
        this.audioStreamIndex = audioStream;
        this.interleaved = interleaved;

        /* Start streaming (because we're supposed to be playing). */
        if (this.playing) {
            this.startSetStreams();
        }
    }

    /**
     * Start streaming.
     */
    start(): void {
        if (this.playing) {
            return;
        }
        this.playing = true;

        /* Start streaming (because we're not already.). */
        if (this.baseUrl) {
            this.startSetStreams();
        }
    }

    /**
     * Stop streaming.
     */
    stop(): void {
        if (!this.playing) {
            return;
        }
        this.playing = false;

        /* Stop the underlying media elements. */
        this.video.pause();
        if (this.audio) {
            this.audio.pause();
        }

        /* Clean up. */
        this.cleaup();
    }

    /**
     * Set the mutedness of the stream.
     */
    setMuted(muted: boolean): void {
        if (this.audio) {
            this.audio.muted = muted;
            if (!muted && !this.video.paused) {
                this.audio.play();
            }
        }
        else {
            this.video.muted = muted;
        }
    }

    /**
     * Get the mutedness of the stream.
     */
    getMuted(): boolean {
        return this.audio ? this.audio.muted : this.video.muted;
    }

    /**
     * Called when a new stream starts playing.
     */
    onNewStreamStart: (() => void) | null = null;

    /**
     * Called when a quality downgrade is recommended.
     */
    onRecommendDowngrade: (() => void) | null = null;

    private cleaup(): void {
        if (this.segmentDownloader) {
            this.segmentDownloader.stop();
        }
        this.videoMediaSource = null;
        this.audioMediaSource = null;
        this.videoStream = null;
        this.audioStream = null;
    }

    /**
     * Start streaming with the source previously given by setSource().
     */
    private startSetStreams(): void {
        /* Fetch the manifest, stream info and initial data. Once we have it, set up streaming. */
        const manifestPromise: Promise<string> = fetch(`${this.baseUrl}/manifest.mpd`).
            then((response: Response): Promise<string> =>
        {
            return response.text();
        });
        if (this.audioStreamIndex === null) {
            Promise.all([manifestPromise, this.getStreamInfoAndInit(this.videoStreamIndex)]).
                then((fetched: [string, [any, ArrayBuffer]]) =>
            {
                this.setupStreams(fetched[0], fetched[1][0], fetched[1][1]);
            });
        }
        else {
            Promise.all([manifestPromise, this.getStreamInfoAndInit(this.videoStreamIndex),
                         this.getStreamInfoAndInit(this.audioStreamIndex)]).
                then((fetched: [string, [any, ArrayBuffer], [any, ArrayBuffer]]) =>
            {
                this.setupStreams(fetched[0], fetched[1][0], fetched[1][1], fetched[2][0], fetched[2][1]);
            });
        }
    }

    private setupStreams(manifest: string, videoInfo: any, videoInit: ArrayBuffer, audioInfo: any = null,
                         audioInit: ArrayBuffer | null = null): void {
        /* Clean up whatever already existed. */
        this.cleaup();

        /* Create afresh. */
        this.setMediaSources((): void => {
            // New streams.
            this.videoStream = new Stream(this.videoMediaSource!, videoInit,
                                          this.getMineForStream(manifest, this.videoStreamIndex), (): void => {
                this.video.play();
                if (this.audio && !this.audio.muted) {
                    this.audio.play();
                }

                if (this.onNewStreamStart) {
                    this.onNewStreamStart();
                }
            });
            if (audioInfo && this.interleaved) {
                this.audioStream =
                    new Stream(this.audio ? this.audioMediaSource! : this.videoMediaSource!,
                               audioInit!, this.getMineForStream(manifest, this.audioStreamIndex));
            }

            // Segment downloader.
            const streams = [this.videoStream];
            if (this.audioStream && this.interleaved) {
                streams.push(this.audioStream);
            }
            this.segmentDownloader =
                new SegmentDownloader(videoInfo, this.interleaved, streams, this.videoStreamIndex,
                                      this.baseUrl!, this.segmentDuration, this.segmentPreavailability);
        }, audioInfo !== null);
    }

    private setMediaSources(onMediaSourceReady: () => void, withAudio: boolean, recall: boolean = false): void {
        /* Create new media sources and bind them to the media elements. */
        if (!recall) {
            this.videoMediaSource = new MediaSource();
            if (this.audio) {
                this.audioMediaSource = new MediaSource();
            }

            this.video.src = URL.createObjectURL(this.videoMediaSource);
            if (withAudio && this.audio) {
                this.audio.src = URL.createObjectURL(this.audioMediaSource);
            }
        }

        /* Wait for the media sourecs to be ready. */
        if (this.videoMediaSource!.readyState != 'open') {
            const callback = () => {
                this.videoMediaSource!.removeEventListener('opensource', callback);
                this.setMediaSources(onMediaSourceReady, withAudio, true);
            };
            this.videoMediaSource!.addEventListener('sourceopen', callback);
            return;
        }
        if (withAudio && this.audio && this.audioMediaSource!.readyState != 'open') {
            const callback = () => {
                this.audioMediaSource!.removeEventListener('opensource', callback);
                this.setMediaSources(onMediaSourceReady, withAudio, true);
            };
            this.audioMediaSource!.addEventListener('sourceopen', callback);
            return;
        }

        /* Do whatever it is we were wanting to do after. */
        // TODO: I suspect this could/should be a promise.
        onMediaSourceReady();
    }

    /**
     * Download stream info JSON and initializer stream.
     *
     * @param baseUrl Base URL for the stream.
     * @param index Stream index.
     */
    private getStreamInfoAndInit(index: number): Promise<[any, ArrayBuffer]> {
        return Promise.all([
            fetch(`${this.baseUrl}/chunk-stream${index}-index.json`),
            fetch(`${this.baseUrl}/init-stream${index}.m4s`)
        ]).then((responses: Array<Response>): Promise<[any, ArrayBuffer]> => {
            return Promise.all([responses[0]!.json(), responses[1]!.arrayBuffer()]);
        });
    }

    /**
     * Extract the full mimetype from a DASH manifest.
     *
     * This uses string manipulation to read the manifest produced by FFMPEG rather than a full XML parsing, but that's
     * OK for our purposes.
     *
     * @param manifest The DASH manifest.
     * @param stream The stream index.
     * @return The mimetype to give to MSE for the given stream.
     */
    private getMineForStream(manifest: string, stream: number): string {
        const re = new RegExp(`<Representation id="${stream}" mimeType="([^"]*)" codecs="([^"]*)"`);
        const match = manifest.match(re);
        return `${match![1]}; codecs="${match![2]}"`; // TODO: Error handling.
    }

    private readonly video: HTMLVideoElement;
    private readonly audio: HTMLAudioElement;
    private readonly segmentDuration: number;
    private readonly segmentPreavailability: number;

    private videoMediaSource: MediaSource | null = null;
    private audioMediaSource: MediaSource | null = null;

    private baseUrl: string | null = null;
    private videoStreamIndex: number = 0;
    private audioStreamIndex: number = 0;
    private interleaved: boolean = false;

    private segmentDownloader: SegmentDownloader | null = null;
    private videoStream: Stream | null = null;
    private audioStream: Stream | null = null;

    private playing: boolean = false;
}
