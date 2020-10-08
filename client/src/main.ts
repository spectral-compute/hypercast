import * as manifest from './manifest.js';
import * as stream from './stream.js';
import * as util from './util.js';

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

    addVideoData(s: ReadableStream): void {
        this.videoStreams.addStream(s);
    }

    mediaSource: MediaSource = new MediaSource();
    videoSource: SourceBuffer = null;
    mediaElement: HTMLMediaElement;
    videoStreams: stream.MultiStreamReader;
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

    const [segmentIndex, offsetInSegment] = manifestInfo.videoRepresentations[0].getSegmentIndexAndOffsetForNow(clock);
    console.log('Segment index: ' + segmentIndex + ', offset: ' + offsetInSegment + ' ms');
}
