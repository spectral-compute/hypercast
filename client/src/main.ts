import * as bufferctrl from "./bufferctrl";
import * as stream from "./stream";

interface VideoConfig {
    codec: string,
    bitrate: number,
    width: number,
    height: number
}

interface AudioConfig {
    codec: string,
    bitrate: number
}

interface ServerInfo {
    angles: {name: string, path: string}[],
    segmentDuration: number,
    segmentPreavailability: number,
    videoConfigs: VideoConfig[],
    audioConfigs: AudioConfig[],
    avMap: [number, number][]
}

export class Player {
    /**
     * Start playing video.
     *
     * None of the other methods of this object are valid until onInit has been called.
     *
     * @param infoUrl The URL to the server's info JSON object.
     * @param video The video tag to play video into.
     * @param video The audio tag to play audio into. If null, the video tag will be used, but a separate audio tag
     *              allows for better buffer control.
     * @param onInit Called when initialization is done.
     * @param verbose Whether or not to be verbose.
     */
    constructor(infoUrl: string, video: HTMLVideoElement, audio: HTMLAudioElement | null,
                onInit: (() => void) | null = null, verbose: boolean = false) {
        if (verbose) {
            console.log("Start up at " + new Date(Date.now()).toISOString());
        }

        this.verbose = verbose;
        this.video = video;
        this.audio = audio;

        /* Asynchronous stuff. Start by fetching the video server info. */
        fetch(infoUrl).then((response: Response) => {
            if (response.status !== 200) {
                throw Error(`Fetching INFO JSON gave HTTP status code ${response.status}`);
            }
            return response.json();
        }).then((serverInfo: ServerInfo): void => {
            /* Extract and parse the server info. */
            this.serverInfo = serverInfo;
            const urlPrefix = infoUrl.replace(/(?<=^([^:]+:[/]{2})[^/]+)[/].*$/, "");

            // Extract angle information.
            for (const angle of this.serverInfo.angles) {
                this.angleOptions.push(angle.name);
                this.angleUrls.push(urlPrefix + angle.path);
            }

            // Extract quality information.
            this.serverInfo.videoConfigs.forEach((value: VideoConfig) => {
                this.qualityOptions.push([value.width, value.height]);
            });

            /* Start muted. This helps prevent browsers from blocking the auto-play. */
            video.muted = true;
            if (audio) {
                audio.muted = true;
            }

            /* Make sure the video is paused (no auto-play). */
            video.pause();
            if (audio) {
                audio.pause();
            }

            /* Create the streamer. */
            this.stream = new stream.MseWrapper(video, audio, this.serverInfo.segmentDuration,
                                                this.serverInfo.segmentPreavailability, (description: string): void => {
                if (this.onError) {
                    this.onError(description);
                }
            });

            /* Set up buffer control for the player. */
            this.bctrl = new bufferctrl.BufferControl(video, audio ? [audio] : [], verbose);

            /* Set up the "on new source playing" event handler. */
            this.stream.onNewStreamStart = (): void => {
                this.callOnStartPlaying(this.electiveChangeInProgress);
                this.electiveChangeInProgress = false;
            };

            /* Set up stream downgrading. */
            this.stream.onRecommendDowngrade = (): void => {
                if (this.quality < this.qualityOptions.length - 1) {
                    this.quality++; // Quality is actually reversed.
                }
                this.updateQualityAndAngle();
            };

            /* Set the quality to an initial default. */
            this.updateQualityAndAngle();

            /* Done :) */
            if (onInit) {
                onInit();
            }
        }).catch((): void => {
            if (this.onError) {
                this.onError("Error initializing player");
            }
        });
    }

    /**
     * Start playing video.
     */
    start(): void {
        this.stream!.start();
        this.bctrl!.start();

        /* Verbose logging. */
        if (!this.verbose) {
            return;
        }
        this.verboseInterval = setInterval((): void => {
            this.printVerboseInvervalInfo();
        }, 1000);
    }

    /**
     * Stop playing video.
     *
     * This resets the player back to the state it was in before start() was called.
     */
    stop(): void {
        if (this.verboseInterval) {
            clearInterval(this.verboseInterval);
        }
        this.stream!.stop();
        this.bctrl!.stop();
    }

    /**
     * Get the live server info JSON object that set up this player.
     */
    getServerInfo(): ServerInfo {
        return this.serverInfo;
    }

    /**
     * Get the list of possible angle settings.
     *
     * This can change whenever the onLoaded event fires.
     *
     * @return A list of angle names.
     */
    getAngleOptions(): string[] {
        return this.angleOptions;
    }

    /**
     * Get the index of the current angle.
     *
     * This can change whenever the onStartPlaying event fires. This is not valid between calls to setAngle() and the
     * onStartPlaying firing.
     *
     * @return The index of the new angle to play. This indexes the options returned by getAngleOptions().
     */
    getAngle(): number {
        return this.angle;
    }

    /**
     * Change the angle.
     *
     * @param index The index of the new angle to play. This indexes the options returned by getAngleOptions().
     */
    setAngle(index: number): void {
        if (index === this.angle) {
            this.callOnStartPlaying(true);
            return;
        }

        this.electiveChangeInProgress = true;
        this.angle = index;
        this.updateQualityAndAngle();
    }

    /**
     * Get the list of possible quality settings.
     *
     * This can change whenever the onStartPlaying event fires. This is not valid between calls to getAngle() and the
     * onStartPlaying firing.
     *
     * @return A list of tuples of (width, height) describing each quality setting.
     */
    getQualityOptions(): [number, number][] {
        return this.qualityOptions;
    }

    /**
     * Get the index of the current quality.
     *
     * This can change whenever the onStartPlaying event fires. This is not valid between calls to setAngle() and
     * setQuality(), and the onStartPlaying firing.
     *
     * @return The index of the playing quality. This indexes the options returned by getQualityOptions().
     */
    getQuality(): number {
        return this.quality;
    }

    /**
     * Change the playing quality setting.
     *
     * @param index The index of the new quality to play. This indexes the options returned by getQualityOptions().
     */
    setQuality(index: number): void {
        if (index === this.quality) {
            this.callOnStartPlaying(true);
            return;
        }

        this.electiveChangeInProgress = true;
        this.quality = index;
        this.updateQualityAndAngle();
    }

    /**
     * Get the list of possible latency settings.
     *
     * @return A list of strings describing each latency setting.
     */
    getLatencyOptions(): string[] {
        return ["Up to 2s (more smooth)", "Up to 1s (less smooth)"];
    }

    /**
     * Determine whether or not user latency control is available.
     *
     * This can change whenever the onStartPlaying event fires.
     *
     * @return True if user latency control applies to the playing video, and false otherwise.
     */
    getLatencyAvailable(): boolean {
        return this.quality < this.qualityOptions.length - 1;
    }

    /**
     * Get the index of the current quality.
     *
     * @return The index of the current latency setting. This indexes the options returned by getLatencyOptions().
     */
    getLatency(): number {
        return this.latency;
    }

    /**
     * Change the playing quality setting.
     *
     * @param index The index of the new latency to use. This indexes the options returned by getLatencyOptions().
     */
    setLatency(index: number): void {
        this.latency = index;
        this.updateLatency();
    }

    /**
     * Determine whether or not the currently playing video has audio.
     *
     * This can change whenever the onStartPlaying event fires.
     */
    hasAudio(): boolean {
        return this.audioStream !== null;
    }

    /**
     * Determine whether or not the audio is muted.
     *
     * @return muted True if the audio is muted, and false otherwise.
     */
    getMuted(): boolean {
        return this.stream!.getMuted();
    }

    /**
     * Sets whether or not the audio is muted.
     *
     * @param muted True if the audio should be muted, and false otherwise.
     */
    setMuted(muted: boolean): void {
        this.stream!.setMuted(muted);
    }

    /**
     * Called whenever the media source changes (i.e: change of quality or change of angle).
     *
     * Note that a call to setAngle() or setQuality() will always call this event to fire, even if no actual change
     * occurred.
     *
     * @param elective Whether or not the change was requested by setAngle() or setQuality().
     */
    onStartPlaying: ((elective: boolean) => void) | null = null;

    /**
     * Called when an error occurs.
     */
    onError: ((description: string) => void) | null = null;

    /**
     * Convenience method for calling onStartPlaying only if it's been registered.
     */
    private callOnStartPlaying(elective: boolean): void {
        if (this.onStartPlaying) {
            this.onStartPlaying(elective);
        }
    }

    /**
     * This must be called whenever latency setting changes.
     */
    private updateLatency(): void {
        if (!this.getLatencyAvailable()) {
            this.bctrl!.setSaferLatency();
            return;
        }
        switch (this.latency) {
            case 0:
                this.bctrl!.setLowLatency();
                break;
            case 1:
                this.bctrl!.setUltraLowLatency();
                break;
        }
    }

    /**
     * This must be called whenever the current quality or angle changes.
     */
    private updateQualityAndAngle(): void {
        /* We, in fact, also need to update the buffer control settings. */
        this.updateLatency();

        /* Figure out which streams the quality corresponds to. */
        if (this.serverInfo.avMap.length > 0) {
            if (this.quality < this.serverInfo.avMap.length) {
                this.videoStream = this.serverInfo.avMap[this.quality]![0];
                this.audioStream = this.serverInfo.avMap[this.quality]![1];
            } else {
                this.videoStream = this.quality;
                this.audioStream = null;
            }
        } else {
            this.videoStream = this.quality;
            this.audioStream = (this.quality < this.serverInfo.audioConfigs.length) ? this.quality : null;
        }

        /* Tell the streamer. */
        this.stream!.setSource(this.angleUrls[this.angle]!, this.videoStream, this.audioStream,
                              this.serverInfo.avMap.length > 0 && this.audioStream !== null);
    }

    /**
     * Prints verbose information about the current state of streaming.
     */
    private printVerboseInvervalInfo(): void {
        if (this.video.paused) {
            return;
        }

        const videoBufferLength = this.bctrl!.getBufferLength();
        const audioBufferLength = this.audio ? this.bctrl!.getBufferLength(0) : NaN;
        const combinedBufferLength = this.audio ? Math.min(videoBufferLength, audioBufferLength) : videoBufferLength;

        const [minBuffer, maxBuffer] = this.bctrl!.getBufferTargets();
        const ewmaBuffer = this.bctrl!.getCatchUpEventEwma();

        const videoUnprunedBufferLength =
            (this.video.buffered.length === 0) ? 0 :
            (this.video.buffered.end(this.video.buffered.length - 1) - this.video.buffered.start(0));
        const audioUnprunedBufferLength =
            (!this.audio || this.audio.buffered.length === 0) ? 0 :
            (this.audio.buffered.end(this.audio.buffered.length - 1) - this.audio.buffered.start(0));

        console.log(
            `Video buffer length: ${videoBufferLength} ms\n` +
            `Audio buffer length: ${audioBufferLength} ms\n` +
            `Combined buffer length: ${combinedBufferLength} ms\n` +
            `Extra video buffer length: ${videoBufferLength - combinedBufferLength} ms\n` +
            `Extra audio buffer length: ${audioBufferLength - combinedBufferLength} ms\n` +
            `Unpruned video buffer length: ${videoUnprunedBufferLength * 1000} ms\n` +
            `Unpruned audio buffer length: ${audioUnprunedBufferLength * 1000} ms\n` +
            `Video playback rate: ${this.video.playbackRate}\n` +
            `Audio playback rate: ${this.audio ? this.audio.playbackRate : NaN}\n` +
            `AV synchronization offset: ${this.audio ? this.bctrl!.getSecondarySync(0) : 0} ms\n` +
            `Buffer targets: ${minBuffer} ms - ${maxBuffer} ms (${ewmaBuffer}\n`
        );
    }

    // Stuff from the constructor.
    private readonly verbose: boolean;
    private readonly video: HTMLVideoElement;
    private readonly audio: HTMLAudioElement | null;

    // Worker objects.
    private stream: stream.MseWrapper | null = null;
    private bctrl: bufferctrl.BufferControl | null = null;
    private verboseInterval: number | null = null;

    // Server information.
    private serverInfo!: ServerInfo;
    private readonly angleUrls: string[] = [];
    private readonly angleOptions: string[] = [];
    private readonly qualityOptions: [number, number][] = []; // Map: quality -> (width,height).

    // Current settings.
    private angle: number = 0;
    private quality: number = 0;
    private latency: number = 0;

    // Derived settings.
    private videoStream: number | null = null;
    private audioStream: number | null = null;

    // State machine.
    private electiveChangeInProgress: boolean = false;
}
