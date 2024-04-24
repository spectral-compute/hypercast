import {TimestampInfo} from "./Deinterleave";
import {API, waitForEvent} from "live-video-streamer-common";
import {assertType} from "@ckitching/typescript-is";
import {SegmentDownloader, ReceivedInfo} from "./SegmentDownloader";
import {getMediaSource} from "../MediaSource";
import {getFullMimeType, Stream, StreamStartEvent} from "../Stream";
import {EventDispatcher} from "../EventDispatcher";
import {PlayerErrorEvent} from "../Player";

/**
 * WTF is this
 */
export class MseTimestampEvent extends Event {
    constructor(public timestampInfo: TimestampInfo) {
        super("error");
    }
}

/**
 * WTF is this
 */
export class MseReceivedEvent extends Event {
    constructor(public receivedInfo: ReceivedInfo) {
        super("received");
    }
}

/**
 * WTF is this
 */
export class MseControlChunkEvent extends Event {
    constructor(public data: ArrayBuffer,

                // Should be enum?
                public controlChunkType: number) {
        super("control_chunk");
    }
}

export interface MseEventMap {
    "timestamp": MseTimestampEvent,
    "received": MseReceivedEvent,
    "control_chunk": MseControlChunkEvent,
    "error": PlayerErrorEvent,
    "start": StreamStartEvent
}

export class MseWrapper extends EventDispatcher<keyof MseEventMap, MseEventMap> {
    constructor(
        private readonly video: HTMLVideoElement,
        private readonly segmentDurationMS: number,
        private readonly segmentPreavailability: number
    ) {
        super();
    }

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
        this.cleanup();
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

    private cleanup(): void {
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

            this.dispatchEvent(new PlayerErrorEvent(e));
        }
    }

    private async setupStreams(manifest: string, videoInfo: any, videoInit: ArrayBuffer, audioInfo: any = null,
                               audioInit: ArrayBuffer | null = null): Promise<void> {
        /* Clean up whatever already existed. */
        this.cleanup();

        /* Validate. */
        assertType<API.SegmentIndexDescriptor>(videoInfo);
        if (audioInfo !== null) {
            assertType<API.SegmentIndexDescriptor>(audioInfo);
        }

        /* Create afresh. */
        this.videoMediaSource = getMediaSource();
        this.video.src = URL.createObjectURL(this.videoMediaSource);
        if (this.videoMediaSource.readyState !== "open") {
            await waitForEvent("sourceopen", this.videoMediaSource);
        }

        // New streams.
        this.videoStream = new Stream(this.videoMediaSource!);
        this.videoStream.on("error", (e) => this.dispatchEvent(new PlayerErrorEvent(e.e)));
        this.videoStream.on("start", () => {
            this.video.play().then(() => {
                // Success: propagate the start event to users.
                this.dispatchEvent(new StreamStartEvent());
            }).catch(e => {
                // The following errors are documented, and may be propagated from here:
                // NotAllowedError:
                //   The user agent (browser) or operating system doesn't allow playback of media in the current context
                //   or situation. This may happen, for example, if the browser requires the user to explicitly start
                //   media playback by clicking a "play" button.
                // NotSupportedError:
                //   The media source (which may be specified as a MediaStream , MediaSource , Blob , or File , for
                //   example) doesn't represent a supported media format.
                this.dispatchEvent(new ErrorEvent(e));
            });
        });

        this.videoStream.startSegmentSequence({mimeType: this.getMimeForStream(manifest, this.videoStreamIndex),
                                               init: videoInit, segmentDuration: this.segmentDurationMS}, 0);

        if (audioInfo && this.interleaved) {
            this.audioStream = new Stream(this.videoMediaSource!);
            this.audioStream.on("error", (e) => this.dispatchEvent(new PlayerErrorEvent(e.e)));
            this.audioStream.startSegmentSequence({mimeType: this.getMimeForStream(manifest, this.audioStreamIndex!),
                                                   init: audioInit!, segmentDuration: this.segmentDurationMS}, 0);
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
            this.segmentPreavailability
        );

        // This is really shit design. Conceivably, the segmentDownloader should just dispatch these directly.
        this.segmentDownloader.on("timestamp", (e) => this.dispatchEvent(new MseTimestampEvent(e.timestampInfo)));
        this.segmentDownloader.on("received", (e) => this.dispatchEvent(new MseReceivedEvent(e.receivedInfo)));
        this.segmentDownloader.on("control_chunk", (e) => this.dispatchEvent(new MseControlChunkEvent(e.data, e.controlChunkType)));
        this.segmentDownloader.on("error", (e) => this.dispatchEvent(new PlayerErrorEvent(e.e)));

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
        const match = manifest.match(re); // TODO: Error handling.
        return getFullMimeType(match![1]!, match![2]!);
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
