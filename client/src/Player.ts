import * as BufferCtrl from "./BufferCtrl";
import {TimestampInfo} from "./Deinterleave";
import {ReceivedInfo} from "./Stream";
import * as Stream from "./Stream";
import {DebugHandler} from "./Debug";
import {API} from "live-video-streamer-common";
import {assertType} from "@ckitching/typescript-is";

export class Player {
    /**
     * @param infoUrl The URL to the server's info JSON object. If set to null, the URL is obtained from the streaminfo
     *                GET parameter.
     * @param video The video tag to play video into.
     * @param verbose Whether or not to be verbose.
     */
    constructor(infoUrl: string | null, video: HTMLVideoElement, verbose: boolean = false) {
        // Get the URL from the streaminfo GET parameter if no info URL is given.
        if (infoUrl === null) {
            infoUrl = (new URLSearchParams(window.location.search)).get("streaminfo");
            if (infoUrl === null) {
                throw Error(`Could not get "streaminfo" GET parameter from window location "${window.location.href}".`);
            }
        }

        // Set properties :)
        this.infoUrl = infoUrl;
        this.video = video;
        this.verbose = verbose;
    }

    /**
     * Start playing video.
     *
     * None of the other methods of this object are valid until the promise has completed unless otherwise noted.
     */
    async init(): Promise<void> {
        try {
            if (this.verbose) {
                console.log("Start up at " + new Date(Date.now()).toISOString());
            }

            const response: Response = await fetch(this.infoUrl);
            if (response.status !== 200) {
                throw Error(`Fetching INFO JSON gave HTTP status code ${response.status}`);
            }
            const serverInfo: unknown = await response.json();

            /* Extract and parse the server info. */
            assertType<API.ServerInfo>(serverInfo);
            this.serverInfo = serverInfo;
            const urlPrefix = this.infoUrl.replace(/^((?:([^:]+:[/]{2})[^/]+)?)[/].*$/, "$1");

            // Extract angle information.
            for (const angle of this.serverInfo.angles) {
                this.angleOptions.push(angle.name);
                this.angleUrls.push(urlPrefix + angle.path);
            }

            // Extract quality information.
            this.serverInfo.videoConfigs.forEach((value: API.VideoConfig) => {
                this.qualityOptions.push([value.width, value.height]);
            });

            /* Start muted. This helps prevent browsers from blocking the auto-play. */
            this.video.muted = true;

            /* Make sure the video is paused (no auto-play). */
            this.video.pause();

            /* Create the streamer. */
            this.stream = new Stream.MseWrapper(this.video, this.serverInfo.segmentDuration,
                                                this.serverInfo.segmentPreavailability,
            (timestampInfo: TimestampInfo): void => {
                this.bctrl!.onTimestamp(timestampInfo);
            },
            (recievedInfo: ReceivedInfo): void => {
                this.bctrl!.onRecieved(recievedInfo);
            },
            (description: string): void => {
                if (this.onError) {
                    this.onError(description);
                }
            });

            /* Set up buffer control for the player. */
            this.bctrl = new BufferCtrl.BufferControl(this.video, this.verbose, (): void => {
                if (this.quality < this.qualityOptions.length - 1) {
                    this.quality++; // Quality is actually reversed.
                }
                this.updateQualityAndAngle();
            }, this.debugHandler);

            /* Set up the "on new source playing" event handler. */
            this.stream.onNewStreamStart = (): void => {
                this.bctrl!.onNewStreamStart();
                this.callOnStartPlaying(this.electiveChangeInProgress);
                this.electiveChangeInProgress = false;
            };

            /* Set the quality to an initial default. */
            this.updateQualityAndAngle();
        } catch (ex: any) {
            const e = ex as Error;
            if (this.onError) {
                this.onError(`Error initializing player: ${e.message}`);
            } else {
                throw e;
            }
        }
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
        this.startTime = Date.now();
        this.verboseInterval = window.setInterval((): void => {
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
     * Get the URL from which the info JSON was loaded (or is to be loaded).
     *
     * This method can be called before init().
     */
    getInfoUrl(): string {
        return this.infoUrl;
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
     * @internal
     *
     * Set the handler to receive the performance and debugging information the player can generate.
     */
    setDebugHandler(debugHandler: DebugHandler): void {
        if (process.env["NODE_ENV"] !== "development") {
            throw Error("Player.setDebugHandler is for development only.");
        }
        this.debugHandler = debugHandler;
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
     * This must be called whenever the current quality or angle changes.
     */
    private updateQualityAndAngle(): void {
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
                               this.quality < this.serverInfo.avMap.length);

        /* Update the buffer control parameters. */
        this.bctrl!.setBufferControlParameters(this.serverInfo.videoConfigs[this.videoStream]!.bufferCtrl);
    }

    /**
     * Prints verbose information about the current state of streaming.
     */
    private printVerboseInvervalInfo(): void {
        if (this.video.paused) {
            return;
        }

        const bufferLength = this.bctrl!.getBufferLength();
        const maxBuffer = this.bctrl!.getBufferTarget();
        const catchUpStats = this.bctrl!.getCatchUpStats();

        const videoUnprunedBufferLength =
            (this.video.buffered.length === 0) ? 0 :
            (this.video.buffered.end(this.video.buffered.length - 1) - this.video.buffered.start(0));

        const netStats = this.bctrl!.getNetworkTimingStats();
        const sNetStats = this.bctrl!.getNetworkTimingStats(true);

        console.log(
            `Run time: ${(Date.now() - this.startTime!) / 1000} s\n` +
            `Buffer length: ${bufferLength} ms\n` +
            `Unpruned buffer length: ${videoUnprunedBufferLength * 1000} ms\n` +
            `Video playback rate: ${this.video.playbackRate}\n` +
            `Network delay history length: count = ${netStats.historyLength} (${sNetStats.historyLength})\n` +
            `                              time  = ${netStats.historyAge / 1000} s\n` +
            `Delay normal: µ = ${netStats.delayMean} ms (${sNetStats.delayMean} ms)\n` +
            `              σ = ${netStats.delayStdDev} ms (${sNetStats.delayStdDev} ms)\n` +
            `Delay deciles: min             = ${netStats.delayMin} ms (${sNetStats.delayMin} ms)\n` +
            `               10th percentile = ${netStats.delayPercentile10} ms (${sNetStats.delayPercentile10} ms)\n` +
            `               median          = ${netStats.delayMedian} ms (${sNetStats.delayMedian} ms)\n` +
            `               90th percentile = ${netStats.delayPercentile90} ms (${sNetStats.delayPercentile90} ms)\n` +
            `               max             = ${netStats.delayMax} ms (${sNetStats.delayMax} ms)\n` +
            `Jitter normal: µ = ${netStats.delayMean - netStats.delayMin} ms` +
                              ` (${sNetStats.delayMean - sNetStats.delayMin} ms)\n` +
            `Jitter deciles: 10th percentile = ${netStats.delayPercentile10 - netStats.delayMin} ms` +
                                             ` (${sNetStats.delayPercentile10 - sNetStats.delayMin} ms)\n` +
            `                median          = ${netStats.delayMedian - netStats.delayMin} ms` +
                                             ` (${sNetStats.delayMedian - sNetStats.delayMin} ms)\n` +
            `                90th percentile = ${netStats.delayPercentile90 - netStats.delayMin} ms` +
                                             ` (${sNetStats.delayPercentile90 - sNetStats.delayMin} ms)\n` +
            `                max             = ${netStats.delayMax - netStats.delayMin} ms` +
                                             ` (${sNetStats.delayMax - sNetStats.delayMin} ms)\n` +
            `Catch up events: n = ${catchUpStats.eventCount}\n` +
            `                 µ = ${catchUpStats.averageTimeBetweenEvents / 1000} s\n` +
            `Buffer target: ${maxBuffer} ms\n` +
            `Server parameters: ${JSON.stringify(this.bctrl!.getServerParams())}\n`
        );
    }

    // Stuff from the constructor.
    private readonly verbose: boolean;
    private readonly video: HTMLVideoElement;
    private readonly infoUrl: string;

    // Worker objects.
    private stream: Stream.MseWrapper | null = null;
    private bctrl: BufferCtrl.BufferControl | null = null;
    private verboseInterval: number | null = null;

    // Server information.
    private serverInfo!: API.ServerInfo;
    private readonly angleUrls: string[] = [];
    private readonly angleOptions: string[] = [];
    private readonly qualityOptions: [number, number][] = []; // Map: quality -> (width,height).

    // Current settings.
    private angle: number = 0;
    private quality: number = 0;

    // Derived settings.
    private videoStream: number | null = null;
    private audioStream: number | null = null;

    // State machine.
    private electiveChangeInProgress: boolean = false;

    // Debug/performance tracking related stuff.
    private startTime: number | null = null;
    private debugHandler: DebugHandler | null = null;
}
