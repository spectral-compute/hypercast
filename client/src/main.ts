import * as manifest from './manifest.js';
import * as stream from './stream.js';
import * as util from './util.js';

declare const segmentPreavailability: number;
declare const manifestUrl: string;

/* Schedules the download of segments. */
class MediaDownloader {
    constructor(representation: manifest.Representation, clock: util.SynchronizedClock,
                callback: (s: ReadableStream) => void) {
        this.callback = callback;
        this.representation = representation;

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
        console.log('Download stopping...');
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
        }, Math.max(this.nextDlTime - Date.now().valueOf(), 1));

        /* Prepare for when the timeout reaches zero. */
        this.currentIndex++;
        this.nextDlTime += this.representation.duration;
    }

    private currentIndex: number;
    private nextDlTime: number;
    private callback: (s: ReadableStream) => void;
    private representation: manifest.Representation;
};

/* A buffer class that wraps SourceBuffer and its event-driven system. */
class MediaBuffer {
    constructor (sourceBuffer: SourceBuffer) {
        this.sourceBuffer = sourceBuffer;
        this.sourceBuffer.onupdateend = (): void => {
            if (this.sourceBuffer.updating || this.pendingBuffers.length == 0) {
                return;
            }

            this.sourceBuffer.appendBuffer(this.pendingBuffers[0]);
            this.pendingBuffers = this.pendingBuffers.slice(1);
        };
    }

    append(data: ArrayBuffer): void {
        if (this.sourceBuffer.updating) {
            this.pendingBuffers.push(data);
        }
        else if (this.pendingBuffers.length > 0) {
            this.pendingBuffers.push(data);
            this.sourceBuffer.appendBuffer(this.pendingBuffers[0]);
            this.pendingBuffers = this.pendingBuffers.slice(1);
        }
        else {
            this.sourceBuffer.appendBuffer(data);
        }
    }

    private sourceBuffer: SourceBuffer;
    private pendingBuffers: Array<ArrayBuffer> = new Array<ArrayBuffer>();
};

/* Class that adapts the media element for us. This does things like create source buffers, and monitor
   buffer sizes. */
class MediaInterface {
    constructor() {
        this.mediaSource = new MediaSource();
        this.mediaElement = document.getElementById('video') as HTMLMediaElement;
        this.mediaElement.src = URL.createObjectURL(this.mediaSource);

        this.mediaElement.onerror = (): void => {
            throw this.mediaElement.error;
        };
    }

    async addVideoSource(representation: manifest.Representation): Promise<void> {
        if (this.videoSource) {
            // TODO
        }

        const initData = await representation.initData;

        this.videoSource = this.mediaSource.addSourceBuffer(representation.mimeType);
        this.videoSource.appendBuffer(await representation.initData);
        this.videoMediaBuffer = new MediaBuffer(this.videoSource);
        this.videoStreams = new stream.MultiStreamReader((data) => {
            //this.videoMediaBuffer.append(initData);
            this.videoMediaBuffer.append(data);
        });
    }

    addVideoData(s: ReadableStream, onDone: (rate: number) => void = () => {}): void {
        this.videoStreams.addStream(s, onDone);
    }

    mediaSource: MediaSource = new MediaSource();
    videoSource: SourceBuffer = null;
    videoMediaBuffer: MediaBuffer;
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
    let mediaDownloader = null;

    mediaInterface.mediaSource.onsourceopen = (): void => {
        if (mediaInterface.videoSource) {
            return;
        }

        console.log('Source open');
        mediaInterface.addVideoSource(manifestInfo.videoRepresentations[0]);

        mediaDownloader = new MediaDownloader(manifestInfo.videoRepresentations[0], clock,
                                              (s: ReadableStream): void => {
            mediaInterface.addVideoData(s);
        });
    };

    /* Keep the video close to the end of the buffer. */
    let playrate = 1;
    setInterval((): void => {
        const me = mediaInterface.mediaElement;

        const ranges = me.buffered;
        if (ranges.length == 0) {
            return;
        }

        const end = ranges.end(ranges.length - 1);
        const playhead = me.currentTime;
        const delay = end - playhead;

        const targetDelay = 0.25;
        const hysteresisDelay = 1.0;
        const maxFastPlayTime = 10;
        const maxFastPlayRate = 4;


        const x = (delay - targetDelay) / (maxFastPlayTime - targetDelay); // Scales [0, 1]
        const y = x * x; // Also scales [0, 1], but a bit more aggressively at longer delays.
        const fastCachupRate = (maxFastPlayRate - 1) * x + 1;

        if (delay <= targetDelay) {
            if (playrate != 1) {
                playrate = 1;
                me.playbackRate = 1;
            }
        }
        else if (delay <= hysteresisDelay) {
            if (playrate > 1) {
                playrate = fastCachupRate;
                me.playbackRate = fastCachupRate;
            }
            else if (playrate != 1) {
                playrate = 1;
                me.playbackRate = 1;
            }
        }
        else if (delay <= maxFastPlayTime) {
            playrate = fastCachupRate;
            me.playbackRate = fastCachupRate;
        }
        else {
            playrate = 1;
            me.playbackRate = 1;
            me.currentTime = end - 0.5;
            console.log('Seeked');
        }
    }, 100);

    /* Some primitive UI :) */
    document.getElementById('play').onclick = (): void => {
        mediaInterface.mediaElement.play();
    };
    document.getElementById('stop').onclick = (): void => {
        if (mediaDownloader !== null) {
            mediaDownloader.stop();
        }
    };

    // Logging.
    const infoDiv = document.getElementById('info');
    setInterval((): void => {
        const me = mediaInterface.mediaElement;
        const info = {
            'Play head delay': ((): string => {
                const ranges = me.buffered;
                if (ranges.length == 0) {
                    return 'None';
                }
                return '' + (ranges.end(ranges.length - 1) - me.currentTime) + ' s';
            })(),
            'Play rate': me.playbackRate + ', ' + playrate,
            'Buffer': ((): string => {
                const ranges = me.buffered;
                if (ranges.length == 0) {
                    return 'None';
                }

                let result: string = '';
                for (let i = 0; i < ranges.length; i++) {
                    const start = ranges.start(i);
                    const end = ranges.end(i);
                    result += '[' + start + ' - ' + end + '] (' + (end - start) + ' s), ';
                }
                return result.slice(0, -2);
            })(),
            'Paused': me.paused,
            'Ready state': me.readyState,
        };

        let table: string = '';
        for (const key in info) {
            table += '<tr><td>' + key + '</td><td>' + info[key] + '</td></tr>\n';
        }
        infoDiv.innerHTML = table;
    }, 100);
}

