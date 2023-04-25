import {DebugHandler} from "./Debug";
import {BufferControl} from "./live/BufferCtrl";
import {TimestampInfo} from "./live/Deinterleave";
import {MseWrapper} from "./live/MseWrapper";
import {ReceivedInfo} from "./live/SegmentDownloader";
import {API} from "live-video-streamer-common";
import {assertType} from "@ckitching/typescript-is";

export interface PlayerEventListeners {
    /** Called when an error occurs */
    onError?: (description: string) => void;

    /** Called when Player's properties change or a video starts playing. */
    onUpdate?: (elective: boolean) => void;

    /**
     * Called with the object given to the send_user_json channel API when it is called.
     */
    onBroadcastObject?: (o: any) => void;

    /**
     * Called with the object given to the send_user_binary channel API when it is called.
     */
    onBroadcastBinary?: (data: ArrayBuffer) => void;

    /**
     * Called with the object given to the send_user_string channel API when it is called.
     */
    onBroadcastString?: (s: string) => void;
}

export class Player {
    /**
     * @param channelIndexUrl The URL to the server's JSON channel index.
     *                        If set to null, the URL is obtained from the "channelindex" GET parameter.
     * @param container The div tag to put a video tag into to play video into.
     * @param listeners Functions which will be called during key streaming events. See {@link PlayerEventListeners}
     */
    constructor(channelIndexUrl: string | null, container: HTMLDivElement, listeners: PlayerEventListeners) {
        // Get the URL from the streaminfo GET parameter if no info URL is given.
        if (channelIndexUrl === null) {
            channelIndexUrl = (new URLSearchParams(window.location.search)).get("channelindex");
            if (channelIndexUrl === null) {
                throw Error(`Could not get "channelindex" parameter from window location "${window.location.href}".`);
            }
        }

        // Set properties :)
        this.channelIndexUrl = channelIndexUrl;
        this.video = container.appendChild(document.createElement("video"));
        this.video.controls = false;

        this.onError = listeners.onError ?? null;
        this.onUpdate = listeners.onUpdate ?? null;
        this.onBroadcastObject = listeners.onBroadcastObject ?? null;
        this.onBroadcastBinary = listeners.onBroadcastBinary ?? null;
        this.onBroadcastString = listeners.onBroadcastString ?? null;
    }

    /**
     * Start playing video.
     *
     * None of the other methods of this object are valid until the promise has completed unless otherwise noted.
     */
    async init(): Promise<void> {
        try {
            // Extract base Url from the channel index url because it might be on a different domain/port.
            this.baseUrl = this.channelIndexUrl.replace(/^((?:([^:]+:[/]{2})[^/]+)?)[/].*$/, "$1");

            await this.loadChannelIndex();

            // Now let's select the first channel.
            // TODO: Make this configurable.
            // TODO: Make this not crash if there are no channels?
            this.channelPath = Object.keys(this.channelIndex)[0]!;

            await this.initCurrentChannel();
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
    start = (): void => {
        this.stream!.start();
        this.bctrl!.start();
    };

    /**
     * Stop playing video.
     *
     * This resets the player back to the state it was in before start() was called.
     */
    stop = (): void => {
        this.stream!.stop();
        this.bctrl!.stop();
    };

    /**
     * Get the URL from which the channel index JSON was loaded (or is to be loaded).
     *
     * This method can be called before init().
     */
    getChannelIndexUrl(): string {
        return this.channelIndexUrl;
    }

    /**
     * Get available channels.
     *
     * This can be changed whenever the channel list is reloaded.
     *
     * The channel paths don't include the base URL of the channel index.
     *
     * @return A map of channel paths (as keys) to channel names (as values, or null if the name is not defined).
     */
    getChannelIndex(): Record<string, string | null> {
        return this.channelIndex;
    }

    /**
     * Get the path from which the info JSON of the current channel was loaded.
     * Does not include the base URL of the channel index.
     */
    getChannelPath(): string {
        return this.channelPath!;
    }

    /**
     * Set channel to play from the channel index.
     *
     * @param channelPath The path relative to the base URL of the channel index.
     */
    setChannelPath(channelPath: string): void {
        this.channelPath = channelPath;
        this.initCurrentChannel();
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
     * This can change whenever the onUpdate event fires.
     * This is not valid between calls to setAngle() and the onUpdate firing.
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
            this.onUpdate?.(true);
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
     * This can change whenever the onUpdate event fires.
     * This is not valid between calls to setChannelPath() and setQuality(), and the onUpdate firing.
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
            this.onUpdate?.(true);
            return;
        }

        this.electiveChangeInProgress = true;
        this.quality = index;
        this.updateQualityAndAngle();
    }

    /**
     * Determine whether or not the currently playing video has audio.
     *
     * This can change whenever the onUpdate event fires.
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

    /** Mute the audio. */
    mute = (): void => {
        this.setMuted(true);
    };

    /** Unmute the audio. */
    unmute = (): void => {
        this.setMuted(false);
    };

    async requestFullscreen(): Promise<void> {
        return this.video.requestFullscreen();
    }

    /**
     * @internal
     *
     * Set the handler to receive the performance and debugging information the player can generate.
     */
    setDebugHandler(debugHandler: DebugHandler | null): void {
        this.debugHandler = debugHandler;
        this.bctrl?.setDebugHandler(debugHandler);
    }

    /**
     * Called whenever the media source changes (i.e: change of quality, change of channel, or of available channels).
     *
     * Note that a call to setChannelPath() or setQuality() will always call this event to fire,
     * even if no actual change occurred.
     *
     * @param elective Whether or not the change was requested by the user.
     */
    onUpdate: ((elective: boolean) => void) | null;

    /**
     * Called when an error occurs.
     */
    onError: ((description: string) => void) | null;

    /**
     * Called with the object given to the send_user_json channel API when it is called.
     */
    onBroadcastObject: ((o: any) => void) | null = null;

    /**
     * Called with the object given to the send_user_binary channel API when it is called.
     */
    onBroadcastBinary: ((data: ArrayBuffer) => void) | null = null;

    /**
     * Called with the object given to the send_user_string channel API when it is called.
     */
    onBroadcastString: ((s: string) => void) | null = null;

    /**
     * Handle control chunks received via interleaves.
     *
     * @param data The data of the control chunk.
     * @param controlChunkType The control chunk type.
     */
    private onControlChunk(data: ArrayBuffer, controlChunkType: number): void {
        try {
            switch (controlChunkType) {
                case API.ControlChunkType.userJsonObject:
                    if (this.onBroadcastObject === null) {
                        throw Error("Received broadcast object, but its handler is not registered.");
                    }
                    this.onBroadcastObject(JSON.parse(new TextDecoder().decode(data)));
                    break;
                case API.ControlChunkType.userBinaryData:
                    if (this.onBroadcastBinary === null) {
                        throw Error("Received broadcast binary data, but its handler is not registered.");
                    }
                    this.onBroadcastBinary(data);
                    break;
                case API.ControlChunkType.userString:
                    if (this.onBroadcastString === null) {
                        throw Error("Received broadcast string, but its handler is not registered.");
                    }
                    this.onBroadcastString(new TextDecoder().decode(data));
                    break;
                default:
                    throw Error(`Unknown control chunk type: ${controlChunkType}.`);
            }
        } catch(ex: any) {
            const e = ex as Error;
            if (this.onError) {
                this.onError(`Bad control chunk: ${e.message}`);
            } else {
                throw e;
            }
        }
    }

    /**
     * (Re)load the channel index.
     */
    private async loadChannelIndex(): Promise<void> {
        // Fetch the channel index
        const channelIndexResponse: Response = await fetch(this.channelIndexUrl);
        if (channelIndexResponse.status !== 200) {
            throw Error(`Fetching channel index JSON gave HTTP status code ${channelIndexResponse.status}`);
        }
        const channelIndex: unknown = await channelIndexResponse.json();

        // Parse the channel index
        assertType<API.ChannelIndex>(channelIndex);
        this.channelIndex = channelIndex;
    }

    /**
     * (Re)initialize the player to play the currently selected channel.
     */
    private async initCurrentChannel(): Promise<void> {
        const wasPlaying = !this.video.paused;

        // Stop the current playback.
        // It will be restarted if it was playing before the channel change.
        this.stream?.stop();
        this.bctrl?.stop();

        const response: Response = await fetch(this.baseUrl + this.channelPath);
        if (response.status !== 200) {
            throw Error(`Fetching INFO JSON gave HTTP status code ${response.status}`);
        }
        const serverInfo: unknown = await response.json();

        /* Extract and parse the server info. */
        assertType<API.ServerInfo>(serverInfo);
        this.serverInfo = serverInfo;

        // Extract angle information.
        this.angleOptions = [];
        this.angleUrls = [];
        for (const angle of this.serverInfo.angles) {
            this.angleOptions.push(angle.name);
            this.angleUrls.push(this.baseUrl + angle.path);
        }

        // Extract quality information.
        this.qualityOptions = [];
        this.serverInfo.videoConfigs.forEach((value: API.VideoConfig) => {
            this.qualityOptions.push([value.width, value.height]);
        });

        /* Start muted. This helps prevent browsers from blocking the auto-play. */
        this.video.muted = true;

        /* Make sure the video is paused (no auto-play). */
        this.video.pause();

        /* Create the streamer. */
        this.stream = new MseWrapper(
            this.video,
            this.serverInfo.segmentDuration,
            this.serverInfo.segmentPreavailability,
            (timestampInfo: TimestampInfo): void => {
                this.bctrl!.onTimestamp(timestampInfo);
            },
            (recievedInfo: ReceivedInfo): void => {
                this.bctrl!.onRecieved(recievedInfo);
            },
            (data: ArrayBuffer, controlChunkType: number): void => {
                this.onControlChunk(data, controlChunkType)
            },
            this.onError ?? ((): void => {}),
        );

        /* Set up buffer control for the player. */
        this.bctrl = new BufferControl(this.video, (): void => {
            if (this.quality < this.qualityOptions.length - 1) {
                this.quality++; // Quality is actually reversed.
            }

            // The stream has severely stalled, so stop playing it and try the new one (if there is a new one). This
            // does not wait for a new segment, or reuse the MSE wrapper/stream/etc.
            this.updateQualityAndAngle();
        }, this.debugHandler);

        /* Set up the "on new source playing" event handler. */
        this.stream.onNewStreamStart = (): void => {
            this.bctrl!.onNewStreamStart();
            this.onUpdate?.(this.electiveChangeInProgress);
            this.electiveChangeInProgress = false;
        };

        /* Set the quality to an initial default. */
        this.quality = 0; // TODO: find the closest quality to the previous one
        this.angle = 0;
        this.updateQualityAndAngle();

        if (wasPlaying) {
            this.start();
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

    // Stuff from the constructor.
    private readonly video: HTMLVideoElement;
    private readonly channelIndexUrl: string;

    // Worker objects.
    private stream: MseWrapper | null = null;
    private bctrl: BufferControl | null = null;

    // Server information.
    private channelIndex!: API.ChannelIndex;
    private baseUrl!: string;
    private serverInfo!: API.ServerInfo;
    private angleUrls: string[] = [];
    private angleOptions: string[] = [];
    private qualityOptions: [number, number][] = []; // Map: quality -> (width,height).

    // Current settings.
    private angle: number = 0;
    private quality: number = 0;
    private channelPath: string | null = null;

    // Derived settings.
    private videoStream: number | null = null;
    private audioStream: number | null = null;

    // State machine.
    private electiveChangeInProgress: boolean = false;

    // Debug/performance tracking related stuff.
    private debugHandler: DebugHandler | null = null;
}
