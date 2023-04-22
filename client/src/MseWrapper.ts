import {TimestampInfo} from "./Deinterleave";
import {API} from "live-video-streamer-common";
import {assertType} from "@ckitching/typescript-is";
import {SegmentDownloader} from "./SegmentDownloader";
import {ReceivedInfo, Stream} from "./Stream";

export class MseWrapper {
    constructor(video: HTMLVideoElement,
        segmentDuration: number,
        segmentPreavailability: number,
        onTimestamp: (timestampInfo: TimestampInfo) => void,
        onReceived: (receivedInfo: ReceivedInfo) => void,
        onError: (description: string) => void,
    ) {
        this.video = video;
        this.segmentDuration = segmentDuration;
        this.segmentPreavailability = segmentPreavailability;
        this.onTimestamp = onTimestamp;
        this.onReceived = onReceived;
        this.onError = onError;
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
    private startSetStreams(): void {
        this.aborter = new AbortController();

        /* Fetch the manifest, stream info and initial data. Once we have it, set up streaming. */
        const manifestPromise: Promise<string> =
            fetch(`${this.baseUrl!}/manifest.mpd`, { signal: this.aborter!.signal })
                .then((response: Response): Promise<string> => {
                    if (response.status !== 200) {
                        throw Error(`Manifest HTTP status code is not OK: ${response.status}`);
                    }
                    return response.text();
                });
        if (this.audioStreamIndex === null) {
            Promise.all([manifestPromise, this.getStreamInfoAndInit(this.videoStreamIndex)])
                .then((fetched: [string, [API.SegmentIndexDescriptor, ArrayBuffer]]) => {
                    this.setupStreams(fetched[0], fetched[1][0], fetched[1][1]);
                }).catch((ex: any): void => {
                    const e = ex as Error;
                    if (e.name === 'AbortError') {
                        return;
                    }
                    this.onError(`Error initializing streams: ${e.message}`);
                });
        } else {
            Promise.all([
                manifestPromise,
                this.getStreamInfoAndInit(this.videoStreamIndex),
                this.getStreamInfoAndInit(this.audioStreamIndex),
            ]).then((fetched: [string, [API.SegmentIndexDescriptor, ArrayBuffer],
                [API.SegmentIndexDescriptor, ArrayBuffer]]) => {
                this.setupStreams(fetched[0], fetched[1][0], fetched[1][1], fetched[2][0], fetched[2][1]);
            }).catch((ex: any): void => {
                const e = ex as Error;
                if (e.name === 'AbortError') {
                    return;
                }
                this.onError(`Error initializing streams: ${e.message}`);
            });
        }
    }

    private setupStreams(manifest: string, videoInfo: any, videoInit: ArrayBuffer, audioInfo: any = null,
                         audioInit: ArrayBuffer | null = null): void {
        /* Clean up whatever already existed. */
        this.cleaup();

        /* Validate. */
        assertType<API.SegmentIndexDescriptor>(videoInfo);
        if (audioInfo !== null) {
            assertType<API.SegmentIndexDescriptor>(audioInfo);
        }

        /* Create afresh. */
        this.setMediaSources((): void => {
            // New streams.
            this.videoStream = new Stream(
                this.videoMediaSource!,
                videoInit,
                this.getMimeForStream(manifest, this.videoStreamIndex),
                this.segmentDuration,
                this.onError,
                (): void => {
                    void this.video.play();
                    if (this.onNewStreamStart) {
                        this.onNewStreamStart();
                    }
                },
            );
            if (audioInfo && this.interleaved) {
                this.audioStream = new Stream(
                    this.videoMediaSource!,
                    audioInit!,
                    this.getMimeForStream(manifest, this.audioStreamIndex!),
                    this.segmentDuration,
                    this.onError,
                );
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
                this.segmentDuration,
                this.segmentPreavailability,
                this.onTimestamp,
                this.onReceived,
                this.onError,
            );
        }, audioInfo !== null);
    }

    private setMediaSources(onMediaSourceReady: () => void, withAudio: boolean, recall: boolean = false): void {
        /* Create new media sources and bind them to the media elements. */
        if (!recall) {
            this.videoMediaSource = new MediaSource();
            this.video.src = URL.createObjectURL(this.videoMediaSource);
        }

        /* Wait for the media sourecs to be ready. */
        if (this.videoMediaSource!.readyState !== "open") {
            const callback = (): void => {
                this.videoMediaSource!.removeEventListener("opensource", callback);
                this.setMediaSources(onMediaSourceReady, withAudio, true);
            };
            this.videoMediaSource!.addEventListener("sourceopen", callback);
            return;
        }

        /* Do whatever it is we were wanting to do after. */
        // TODO: I suspect this could/should be a promise.
        onMediaSourceReady();
    }

    /**
     * Download stream info JSON and initializer stream.
     *
     * @param index Stream index.
     */
    private getStreamInfoAndInit(index: number): Promise<[any, ArrayBuffer]> {
        return Promise.all([
            fetch(`${this.baseUrl!}/chunk-stream${index}-index.json`, { signal: this.aborter!.signal }),
            fetch(`${this.baseUrl!}/init-stream${index}.m4s`, { signal: this.aborter!.signal }),
        ]).then((responses: Response[]): Promise<[any, ArrayBuffer]> => {
            for (const response of responses) {
                if (response.status !== 200) {
                    throw Error(`Fetching stream info or init gave HTTP status code ${response.status}`);
                }
            }
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
    private getMimeForStream(manifest: string, stream: number): string {
        const re = new RegExp(`<Representation id="${stream}" mimeType="([^"]*)" codecs="([^"]*)"`);
        const match = manifest.match(re);
        return `${match![1]!}; codecs="${match![2]!}"`; // TODO: Error handling.
    }

    private readonly video: HTMLVideoElement;
    private readonly segmentDuration: number;
    private readonly segmentPreavailability: number;
    private readonly onTimestamp: (timestampInfo: TimestampInfo) => void;
    private readonly onReceived: (receivedInfo: ReceivedInfo) => void;
    private readonly onError: (description: string) => void;

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
