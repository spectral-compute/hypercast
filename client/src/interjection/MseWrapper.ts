import {SegmentDownloader, SegmentDownloaderItem, StreamInfo} from "./SegmentDownloader";
import {getFullMimeType, Stream} from "../Stream";
import {waitForEvent} from "live-video-streamer-common";
import {EventDispatcher} from "../EventDispatcher";
import {PlayerErrorEvent} from "../Player";

export interface InterjectionMseEventMap {
    "error": PlayerErrorEvent
}

export class MseWrapper extends EventDispatcher<keyof InterjectionMseEventMap, InterjectionMseEventMap>{
    constructor(private readonly mediaElement: HTMLMediaElement,
                private readonly signal: AbortSignal,
                private readonly spawn: (fn: () => void | Promise<void>) => void,
                private readonly download: (url: string, type: "binary" | "json" | "string",
                                            what: string) => Promise<any>) {
        super();
    }

    end(): void {
        //this.videoDownloader!.end();
        //this.audioDownloader!.end();
        this.mediaSource?.endOfStream();
    }

    /**
     * Set the download rate throttling.
     *
     * @param rate The rate, as a proportion of real-time. Null to disable.
     */
    setThrottling(rate: number | null): void {
        this.rate = rate;
        this.videoDownloader?.setThrottling(rate);
        this.audioDownloader?.setThrottling(rate);
    }

    /**
     * Append a video to the download.
     *
     * @param baseUrl The base URL of the streams.
     * @param segmentCount The number of segments in the video.
     * @param segmentDuration The length of each segment.
     * @param videoStreams The set of available video streams.
     * @param audioStreams The set of available audio streams.
     * @return A promise that resolves to the PTS of the start of the video stream.
     */
    async append(baseUrl: string, segmentCount: number, segmentDuration: number,
                 videoStreams: StreamInfo[], audioStreams: StreamInfo[]): Promise<number> {
        await this.init();

        const result = this.videoDownloader!.append(new SegmentDownloaderItem(baseUrl, segmentCount, segmentDuration,
                                                                              videoStreams, 0, this.chooseStream));
        void this.audioDownloader!.append(new SegmentDownloaderItem(baseUrl, segmentCount, segmentDuration,
                                                                    audioStreams, videoStreams.length,
                                                                    this.chooseStream));
        return await result;
    }

    private chooseStream(segmentDownloaderItem: SegmentDownloaderItem): number {
        for (let streamIndex: number = 0; streamIndex < segmentDownloaderItem.streams.length; streamIndex++) {
            const stream: StreamInfo = segmentDownloaderItem.streams[streamIndex]!;
            if (MediaSource.isTypeSupported(getFullMimeType(stream.mimeType, stream.codecs))) {
                return streamIndex;
            }
        }
        throw Error("No useable interjection stream.");

        // TODO: ABR selection rather than always choosing the first usable stream.
    }

    private onRecommendDowngrade(downloader: SegmentDownloader, item: SegmentDownloaderItem): void {
        // TODO
        void downloader;
        void item;
    }

    /**
     * Asynchronous idempotent constructor-like.
     */
    private async init(): Promise<void> {
        /* Don't re-initialize. */
        if (this.mediaSource !== null) {
            return;
        }

        /* Create a media source and wait for it to be connected to the video tag. */
        this.mediaSource = new MediaSource();
        this.mediaElement.src = URL.createObjectURL(this.mediaSource);
        if (this.mediaSource.readyState !== "open") {
            await waitForEvent("sourceopen", this.mediaSource);
        }

        /* Wire up the streams and segment downloaders. */
        this.videoStream = new Stream(this.mediaSource, true, this.mediaElement);
        this.audioStream = new Stream(this.mediaSource, true, this.mediaElement);
        this.videoStream.on("error", (e) => this.dispatchEvent(new PlayerErrorEvent(e.e)));
        this.audioStream.on("error", (e) => this.dispatchEvent(new PlayerErrorEvent(e.e)));

        this.videoDownloader = new SegmentDownloader(this.videoStream, this.signal,
                                                     () => this.waitForSourceBuffers(), this.spawn, this.download,
                                                     (downloader, item) => this.onRecommendDowngrade(downloader, item));
        this.audioDownloader = new SegmentDownloader(this.audioStream, this.signal,
                                                     () => this.waitForSourceBuffers(), this.spawn, this.download,
                                                     (downloader, item) => this.onRecommendDowngrade(downloader, item));

        // Pass on some settings to the downloaders.
        this.videoDownloader.setThrottling(this.rate);
        this.audioDownloader.setThrottling(this.rate);
    }

    /**
     * Wait for both source buffers to be created.
     *
     * This is necessary because adding a SourceBuffer to a MediaSource after it's started receiving data (or at least
     * started playing) doesn't work.
     */
    private async waitForSourceBuffers(): Promise<void> {
        // Wait for both the video and audio source buffers to be created.
        while (this.mediaSource!.sourceBuffers.length < 2) {
            await waitForEvent("addsourcebuffer", this.mediaSource!.sourceBuffers, this.signal);
        }
    }

    private mediaSource: MediaSource | null = null;
    private videoStream: Stream | null = null;
    private audioStream: Stream | null = null;
    private videoDownloader: SegmentDownloader | null = null;
    private audioDownloader: SegmentDownloader | null = null;
    private rate: number | null = null;
}
