import {TimestampInfo} from "./Deinterleave";
import {API, waitForEvent} from "live-video-streamer-common";
import {assertType} from "@ckitching/typescript-is";
import {SegmentDownloader, ReceivedInfo} from "./SegmentDownloader";
import {Stream} from "../Stream";

export class MseWrapper {
    constructor(
        private readonly video: HTMLVideoElement,
        private readonly segmentDurationMS: number,
        private readonly segmentPreavailability: number,
        private readonly onTimestamp: (timestampInfo: TimestampInfo) => void,
        private readonly onReceived: (receivedInfo: ReceivedInfo) => void,
        private readonly onControlChunk: (data: ArrayBuffer, controlChunkType: number) => void,
        private readonly onError: (description: string) => void
    ) {}

    setSource(baseUrl: string, videoStream: number, audioStream: number | null, interleaved: boolean): void {
        /* Stop downloading any existing stream. */
        if (this.aborter) {
            this.aborter.abort();
        }
        this.aborter = null;

        if (this.segmentDownloader) {
            this.segmentDownloader.stop();
        }

        /* If we're setting the source to what it's already set to, do nothing. */
        if (baseUrl === this.baseUrl && videoStream === this.videoStreamIndex &&
            audioStream === this.audioStreamIndex) {
            return;
        }
        this.baseUrl = baseUrl;
        this.videoStreamIndex = videoStream;
        this.audioStreamIndex = audioStream;
        this.interleaved = interleaved;

        /* Start streaming (because we're supposed to be playing). */
        if (this.playing) {
            void this.startSetStreams();
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
            void this.startSetStreams();
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

        /* Clean up. */
        this.cleaup();
    }

    /**
     * Set the mutedness of the stream.
     */
    setMuted(muted: boolean): void {
        this.video.muted = muted;
    }

    /**
     * Get the mutedness of the stream.
     */
    getMuted(): boolean {
        return this.video.muted;
    }

    /**
     * Called when a new stream starts playing.
     */
    onNewStreamStart: (() => void) | null = null;

    private cleaup(): void {
        if (this.aborter) {
            this.aborter.abort();
        }
        this.aborter = null;

        if (this.segmentDownloader) {
            this.segmentDownloader.stop();
        }
        this.videoMediaSource = null;
        this.videoStream = null;
        this.audioStream = null;
    }

    /**
     * Start streaming with the source previously given by setSource().
     */
    private async startSetStreams(): Promise<void> {
        this.aborter = new AbortController();

        /* Fetch the manifest, stream info and initial data. Once we have it, set up streaming. */
        try {
            const manifestResponse = await fetch(`${this.baseUrl!}/manifest.mpd`, { signal: this.aborter!.signal });
            if (manifestResponse.status !== 200) {
                throw Error(`Manifest HTTP status code is not OK: ${manifestResponse.status}`);
            }

            if (this.audioStreamIndex === null) {
                const [manifest, [videoInfo, viedoInit]]: [string, [API.SegmentIndexDescriptor, ArrayBuffer]] =
                    await Promise.all([manifestResponse.text(), this.getStreamInfoAndInit(this.videoStreamIndex)]);
                await this.setupStreams(manifest, videoInfo, viedoInit);
            } else {
                const [manifest, [videoInfo, viedoInit], [audioInfo, audioInit]]:
                    [string, [API.SegmentIndexDescriptor, ArrayBuffer], [API.SegmentIndexDescriptor, ArrayBuffer]] =
                    await Promise.all([manifestResponse.text(), this.getStreamInfoAndInit(this.videoStreamIndex),
                                       this.getStreamInfoAndInit(this.audioStreamIndex)]);
                await this.setupStreams(manifest, videoInfo, viedoInit, audioInfo, audioInit);
            }
        } catch(ex: any) {
            const e = ex as Error;
            if (e.name === 'AbortError') {
                return;
            }
            this.onError(`Error initializing streams: ${e.message}`);
        }
    }

    private async setupStreams(manifest: string, videoInfo: any, videoInit: ArrayBuffer, audioInfo: any = null,
                               audioInit: ArrayBuffer | null = null): Promise<void> {
        /* Clean up whatever already existed. */
        this.cleaup();

        /* Validate. */
        assertType<API.SegmentIndexDescriptor>(videoInfo);
        if (audioInfo !== null) {
            assertType<API.SegmentIndexDescriptor>(audioInfo);
        }

        /* Create afresh. */
        this.videoMediaSource = new MediaSource();
        this.video.src = URL.createObjectURL(this.videoMediaSource);
        if (this.videoMediaSource.readyState !== "open") {
            await waitForEvent("sourceopen", this.videoMediaSource);
        }

        // New streams.
        this.videoStream = new Stream(
            this.videoMediaSource!,
            this.segmentDurationMS / 1_000,
            this.onError,
            (): void => {
                void this.video.play();
                if (this.onNewStreamStart) {
                    this.onNewStreamStart();
                }
            },
        );
        this.videoStream.startSegmentSequence({mimeType: this.getMimeForStream(manifest, this.videoStreamIndex),
                                               init: videoInit}, 0);

        if (audioInfo && this.interleaved) {
            this.audioStream = new Stream(
                this.videoMediaSource!,
                this.segmentDurationMS / 1_000,
                this.onError,
            );
            this.audioStream.startSegmentSequence({mimeType: this.getMimeForStream(manifest, this.audioStreamIndex!),
                                                   init: audioInit!}, 0);
        }

        // Segment downloader.
        const streams = [this.videoStream];
        if (this.audioStream && this.interleaved) {
            streams.push(this.audioStream);
        }
        this.segmentDownloader = new SegmentDownloader(
            videoInfo,
            this.interleaved,
            streams,
            this.videoStreamIndex,
            this.baseUrl!,
            this.segmentDurationMS,
            this.segmentPreavailability,
            this.onTimestamp,
            this.onReceived,
            this.onControlChunk,
            this.onError,
        );
    }

    /**
     * Download stream info JSON and initializer stream.
     *
     * @param index Stream index.
     */
    private async getStreamInfoAndInit(index: number): Promise<[any, ArrayBuffer]> {
        const [indexResponse, initResponse]: [Response, Response] =
            await Promise.all([
                fetch(`${this.baseUrl!}/chunk-stream${index}-index.json`, { signal: this.aborter!.signal }),
                fetch(`${this.baseUrl!}/init-stream${index}.m4s`, { signal: this.aborter!.signal }),
            ]);
        if (indexResponse.status !== 200) {
            throw Error(`Fetching stream index gave HTTP status code ${indexResponse.status}`);
        }
        if (initResponse.status !== 200) {
            throw Error(`Fetching stream initialization segment gave HTTP status code ${initResponse.status}`);
        }
        return await Promise.all([indexResponse.json(), initResponse.arrayBuffer()]);
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
    private getMimeForStream(manifest: string, stream: number): string {
        const re = new RegExp(`<Representation id="${stream}" mimeType="([^"]*)" codecs="([^"]*)"`);
        const match = manifest.match(re);
        return `${match![1]!}; codecs="${match![2]!}"`; // TODO: Error handling.
    }

    private videoMediaSource: MediaSource | null = null;

    private baseUrl: string | null = null;
    private videoStreamIndex: number = 0;
    private audioStreamIndex: number | null = null;
    private interleaved: boolean = false;

    private segmentDownloader: SegmentDownloader | null = null;
    private videoStream: Stream | null = null;
    private audioStream: Stream | null = null;

    private playing: boolean = false;

    private aborter: AbortController | null = null;
}
