import * as manifest from './manifest.js';
import * as stream from './stream.js';
import * as util from './util.js';

declare const segmentPreavailability: number;
declare const manifestUrl: string;

/* Class that adapts the media element for us. This does things like create source buffers, and monitor
   buffer sizes. */
class MediaInterface {
    constructor() {
        this.mediaSource = new MediaSource();
        this.mediaElement = document.getElementById('video') as HTMLMediaElement;
        this.mediaElement.src = URL.createObjectURL(this.mediaSource);
    }

    async addVideoSource(representation: manifest.Representation): Promise<void> {
        if (this.videoSource) {
            // TODO
        }

        this.videoSource = this.mediaSource.addSourceBuffer(representation.mimeType);
        this.videoSource.appendBuffer(await representation.initData);
        this.videoStreams = new stream.MultiStreamReader((data) => {
            this.videoSource.appendBuffer(data);
        });
    }

    addVideoData(s: ReadableStream, onDone: (rate: number) => void = () => {}): void {
        this.videoStreams.addStream(s, onDone);
    }

    mediaSource: MediaSource = new MediaSource();
    videoSource: SourceBuffer = null;
    mediaElement: HTMLMediaElement;
    videoStreams: stream.MultiStreamReader;
};

/* Schedules the download of segments. */
class MediaDownloader {
    constructor(representation: manifest.Representation, clock: util.SynchronizedClock,
                callback: (s: ReadableStream) => void) {
        this.callback = callback;

        /* Figure out where in the stream we are now. */
        const [segmentIndex, offsetInSegment] = representation.getSegmentIndexAndOffsetForNow(clock);
        console.log('Segment index: ' + segmentIndex + ', offset: ' + offsetInSegment + ' ms');

        /* Calculate the time when the next segment becomes pre-available. */
        let timeToNextDl = representation.duration - offsetInSegment - segmentPreavailability;

        // If that time is in the past, then just start the whole process with the next segment.
        if (timeToNextDl < 0) {
            timeToNextDl += representation.duration;
            this.currentIndex = segmentIndex + 1;
        }
        else {
            this.currentIndex = segmentIndex;
        }

        // Now figure out *when* the next download is.
        this.nextDlTime = Date.now().valueOf() + timeToNextDl;

        /* set the downloads going! */
        this.downloadScheduleCycle();
    }

    stop(): void {
        this.nextDlTime = 0;
    }

    private downloadScheduleCycle(): void {
        /* Do nothing if we've stopped. */
        if (this.nextDlTime == 0) {
            return;
        }

        /* Download the current segment. */
        fetch(this.representation.getSegmentUrl(this.currentIndex)).then((response: Response): void => {
            this.callback(response.body);
        });

        /* Schedule the next segment. */
        setTimeout((): void => {
            this.downloadScheduleCycle();
        }, Math.min(this.nextDlTime - Date.now().valueOf(), 1));

        /* Prepare for when the timeout reaches zero. */
        this.currentIndex++;
        this.nextDlTime += this.representation.duration;
    }

    private currentIndex: number;
    private nextDlTime: number;
    private callback: (s: ReadableStream) => void;
    private representation: manifest.Representation;
};

/* Initialization. */
export async function init(): Promise<void> {
    /* Load the manifest. */
    const manifestInfo = new manifest.Manifest(await util.fetchString(manifestUrl));

    /* Create the clock. */
    let clock = new util.SynchronizedClock(await util.fetchString(manifestInfo.timeSyncUrl));

    /* Shuffle video into the media interface. */
    const mediaInterface = new MediaInterface();
    mediaInterface.addVideoSource(manifestInfo.videoRepresentations[0]);

    const mediaDownloader = new MediaDownloader(manifestInfo.videoRepresentations[0], clock,
                                                (s: ReadableStream): void => {
        mediaInterface.addVideoData(s);
    });
}
