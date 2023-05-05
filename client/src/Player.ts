import {DebugHandler} from "./Debug";
import {InterjectionPlayer} from "./interjection/Player";
import {BufferControl} from "./live/BufferCtrl";
import {MseWrapper} from "./live/MseWrapper";
import {API} from "live-video-streamer-common";
import {assertType} from "@ckitching/typescript-is";
import {
    PlayerBinaryBroadcastEvent,
    PlayerErrorEvent, PlayerEventMap,
    PlayerObjectBroadcastEvent, PlayerStringBroadcastEvent,
    PlayerUpdateEvent
} from "./Events";
import {EventDispatcher} from "./EventDispatcher";


/**
 * Options for the Player class that configure available sources.
 */
export interface PlayerSourceOptions {
    /**
     * The URL to the server hosting the streams.
     *
     * If not provided, the player will try to use the current origin.
     * For example, if the player is hosted at `https://example.com:1234/player`,
     * this will default to `https://example.com:1234/`.
     *
     * The server can provide a channel index (at `/channelIndex.json`), which lists available channels.
     * If the channel index is not present, a channel must be specified with the `channel` option.
     *
     * When channel index is not present, channel-selecting capabilities of the player are disabled.
     *
     * The URL is also a base URL for various streams.
     * Paths from the channel index and/or the default channel path will be appended to this URL
     * to access individual streams.
     */
    server?: string;

    /**
     * The path to the default channel, relative to the server URL.
     *
     * If channel index is present on the server, this channel must be present in it.
     * If channel index is not present on the server, this channel must be present on the server.
     */
    channel?: string;

    /**
     * Polling interval for channel index in milliseconds.
     *
     * The player periodically polls the channel index to check if the list of available channels has changed.
     * In case of a change, onUpdate handler would be called (if set), which would allow UI to be updated.
     *
     * If not set, a default value is used.
     * If channel index is not present, this setting is ignored.
     */
    indexPollMs?: number;
}

export class Player extends EventDispatcher<keyof PlayerEventMap, PlayerEventMap> {
    /**
     * @param container The div tag to put a video tag into to play video into.
     * @param source Options for configuring available sources. See {@link PlayerSourceOptions}
     */
    constructor(
        private readonly container: HTMLDivElement,
        source?: PlayerSourceOptions
    ) {
        super();

        const {server = window.location.origin, channel, indexPollMs} = source ?? {};

        this.video = this.makeVideoTag(true);
        this.video.controls = false;

        this.server = server;
        // We assign `string | undefined` here because the user must run `init()` first which will deal with it.
        this.channel = channel!;
        this.indexPollMs = indexPollMs ?? 5_000;
    }

    /**
     * Start playing video.
     *
     * None of the other methods of this object are valid until the promise has completed unless otherwise noted.
     */
    async init(): Promise<void> {
        // This function must set up all the fields that might otherwise be undefined.
        // Pay additional attention to fields marked with "!".

        try {
            await this.loadChannelIndex();

            // Select the channel to be played.

            const defaultChannelSet = !!this.channel;
            const channelIndexAvailable = !!this.channelIndex;
            const channelIndexNotEmpty = channelIndexAvailable && Object.keys(this.channelIndex!).length > 0;

            if (defaultChannelSet) {
                await this.setChannel(this.channel);
            } else if (channelIndexAvailable && channelIndexNotEmpty) {
                // Choose the first channel in the channel index.
                await this.setChannel(Object.keys(this.channelIndex!)[0]!);
            } else {
                throw Error(`No channels available from the channel index and no default channel is set.`);
            }

            if (channelIndexAvailable) {
                // Set up polling for the channel index.
                // TODO: Make sure no cleanup is needed when the player is destroyed.
                window.setInterval(async () => {
                    const oldChannelIndex = this.channelIndex;
                    await this.loadChannelIndex();
                    if (JSON.stringify(oldChannelIndex) !== JSON.stringify(this.channelIndex)) {
                        // The channel index has changed.
                        this.dispatchEvent(new PlayerUpdateEvent(false));
                    }
                }, this.indexPollMs);
            }

        } catch (ex: any) {
            const e = ex as Error;
            if (this.dispatchEvent(new PlayerErrorEvent(e))) {
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
     * Get the URL of the server used.
     */
    getServer(): string {
        return this.server;
    }

    /**
     * Get available channels.
     *
     * This returns the channel list most recently retrieved from the server.
     * The value could change between calls if the server updates and the Player polls it.
     * The channel paths don't include the server URL.
     *
     * @return A map of channel paths (as keys) to channel names (as values, or null if the name is not defined).
     */
    getChannelIndex(): Record<string, string | null> | undefined {
        return this.channelIndex;
    }

    /**
     * Get the path from which the info JSON of the current channel was loaded.
     * Does not include the server URL.
     */
    getChannel(): string {
        return this.channel;
    }

    /**
     * Set channel to play from the channel index.
     *
     * @param channel The path relative to the server URL.
     *
     * @throws When the channel index is available but the channel is not listed in the index.
     */
    async setChannel(channel: string): Promise<void> {
        if (this.channelIndex && !Object.keys(this.channelIndex).includes(channel)) {
            throw Error(`Channel "${channel}" is not in the channel index.`);
        }

        this.channel = channel;
        await this.initCurrentChannel();
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
            this.dispatchEvent(new PlayerUpdateEvent(true));
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
            this.dispatchEvent(new PlayerUpdateEvent(true));
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
        this.debugHandler = debugHandler ?? undefined;
        this.bctrl?.setDebugHandler(debugHandler);
    }

    /**
     * Handle control chunks received via interleaves.
     *
     * @param data The data of the control chunk.
     * @param controlChunkType The control chunk type.
     */
    private onControlChunk(data: ArrayBuffer, controlChunkType: number): void {
        try {
            switch (controlChunkType) {
                case API.ControlChunkType.jsonObject:
                    const j = JSON.parse(new TextDecoder().decode(data));
                    assertType<API.JsonObjectControlChunk>(j);
                    this.onJsonObject(j);
                    break;
                case API.ControlChunkType.userJsonObject:
                    this.dispatchEvent(new PlayerObjectBroadcastEvent(JSON.parse(new TextDecoder().decode(data))));
                    break;
                case API.ControlChunkType.userBinaryData:
                    this.dispatchEvent(new PlayerBinaryBroadcastEvent(data));
                    break;
                case API.ControlChunkType.userString:
                    this.dispatchEvent(new PlayerStringBroadcastEvent(new TextDecoder().decode(data)));
                    break;
                default:
                    throw Error(`Unknown control chunk type: ${controlChunkType}.`);
            }
        } catch(ex: any) {
            const e = ex as Error;
            if (this.dispatchEvent(new PlayerErrorEvent(e))) {
                throw e;
            }
        }
    }

    /**
     * (Re)load the channel index.
     */
    private async loadChannelIndex(): Promise<void> {
        const channelIndexPath = "/channelIndex.json";

        // Fetch the channel index
        const channelIndexResponse: Response = await fetch(this.server + channelIndexPath);
        if (channelIndexResponse.status === 200) {
            const channelIndex: unknown = await channelIndexResponse.json();
            assertType<API.ChannelIndex>(channelIndex);
            this.channelIndex = channelIndex;
        }

        // If the channel index is not available, it's fine as far as this function is concerned.
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

        const response: Response = await fetch(this.server + this.channel);
        if (response.status !== 200) {
            throw Error(`Fetching INFO JSON for channel ${this.channel} gave HTTP status code ${response.status}`);
        }
        const channelInfo: unknown = await response.json();

        /* Extract and parse the server info. */
        assertType<API.ServerInfo>(channelInfo);
        this.channelInfo = channelInfo;

        // Extract angle information.
        this.angleOptions = [];
        this.angleUrls = [];
        for (const angle of this.channelInfo.angles) {
            this.angleOptions.push(angle.name);
            this.angleUrls.push(this.server + angle.path);
        }

        // Extract quality information.
        this.qualityOptions = [];
        this.channelInfo.videoConfigs.forEach((value: API.VideoConfig) => {
            this.qualityOptions.push([value.width, value.height]);
        });

        /* Start muted. This helps prevent browsers from blocking the auto-play. */
        this.video.muted = true;

        /* Make sure the video is paused (no auto-play). */
        this.video.pause();

        /* Create the streamer. */
        this.stream = new MseWrapper(
            this.video,
            this.channelInfo.segmentDuration,
            this.channelInfo.segmentPreavailability
        );
        this.stream.on("timestamp", (e) => this.bctrl!.onTimestamp(e.timestampInfo));
        this.stream.on("received", (e) => this.bctrl!.onRecieved(e.receivedInfo));
        this.stream.on("control_chunk", (e) => this.onControlChunk(e.data, e.controlChunkType));
        this.stream.on("error", (e) => this.dispatchEvent(e));

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
        this.stream.on("start", () => {
            this.bctrl!.onNewStreamStart();
            this.dispatchEvent(new PlayerUpdateEvent(this.electiveChangeInProgress));
            this.electiveChangeInProgress = false;
        });

        /* Set the quality to an initial default. */
        this.quality = 0; // TODO: find the closest quality to the previous one
        this.angle = 0;
        this.updateQualityAndAngle();

        if (wasPlaying) {
            this.start();
        }
    }

    /**
     * Handle control chunks of type jsonObject.
     *
     * @param o The chunk data, converted from a JSON object.
     */
    private onJsonObject(o: API.JsonObjectControlChunk): void {
        switch (o.type) {
            case "interject":
                const interjectionRequest = o.content;
                assertType<API.InterjectionRequest>(interjectionRequest);
                this.onInterjectionRequest(interjectionRequest);
                break;
            default:
                throw Error(`Unknown JSON object control chunk type: ${o.type}.`);
        }
    }

    /**
     * Handle interjection requests.
     */
    private onInterjectionRequest(request: API.InterjectionRequest): void {
        const interjectionVideo: HTMLVideoElement = this.makeVideoTag();
        const interjectionPlayer = new InterjectionPlayer(
            request, this.video, interjectionVideo, this.start, this.stop,
            null, null
        );
        interjectionPlayer.on("error", (e) => this.dispatchEvent(e));

        // TODO: WHAT THE FUCK
        void interjectionPlayer;
    }

    /**
     * This must be called whenever the current quality or angle changes.
     */
    private updateQualityAndAngle(): void {
        /* Figure out which streams the quality corresponds to. */
        if (this.channelInfo.avMap.length > 0) {
            if (this.quality < this.channelInfo.avMap.length) {
                this.videoStream = this.channelInfo.avMap[this.quality]![0];
                this.audioStream = this.channelInfo.avMap[this.quality]![1];
            } else {
                this.videoStream = this.quality;
                this.audioStream = null;
            }
        } else {
            this.videoStream = this.quality;
            this.audioStream = (this.quality < this.channelInfo.audioConfigs.length) ? this.quality : null;
        }

        /* Tell the streamer. */
        this.stream!.setSource(this.angleUrls[this.angle]!, this.videoStream, this.audioStream,
                               this.quality < this.channelInfo.avMap.length);

        /* Update the buffer control parameters. */
        this.bctrl!.setBufferControlParameters(this.channelInfo.videoConfigs[this.videoStream]!.bufferCtrl);
    }


    /* ============= *
     * Configuration *
     * ============= */

    // The video element we are playing to (owned by this class).

    /**
     * Make a video tag for use with this player.
     *
     * This method factors out the creation, styling, and so on of the video tags.
     *
     * @param visible Whether to create the video element as visible (true), or hidden (false, the default).
     */
    private makeVideoTag(visible: boolean = false): HTMLVideoElement {
        const video: HTMLVideoElement = this.container.appendChild(document.createElement("video"));
        video.controls = false; // Native controls would mess up buffer control.
        video.style.display = visible ? "" : "none";
        return video;
    }

    // Stuff from the constructor.
    private readonly video: HTMLVideoElement;

    // The server that all the data is coming from.
    private readonly server: string;
    // Currently selected channel, path relative to the `server`.
    private channel!: string;
    // How often to poll the channel index for changes.
    private readonly indexPollMs: number;

    /* ============== *
     * Worker objects *
     * ============== */

    private stream!: MseWrapper;
    private bctrl!: BufferControl;

    /* ================== *
     * Server information *
     * ================== */

    // Cached channel index.
    private channelIndex?: API.ChannelIndex;

    /* ===================================== *
     * Information about the current channel *
     * ===================================== */

    private channelInfo!: API.ServerInfo;
    private angleUrls: string[] = [];
    private angleOptions: string[] = [];
    private qualityOptions: [number, number][] = []; // Map: quality -> (width,height).

    /* =============================== *
     * Current configuration, relative *
     * to the current channel          *
     * =============================== */

    private angle: number = 0;
    private quality: number = 0;

    /* ===================== *
     * Derived configuration *
     * ===================== */

    private videoStream: number | null = null;
    private audioStream: number | null = null;

    /* ============= *
     * State machine *
     * ============= */

    private electiveChangeInProgress: boolean = false;

    /* =================== *
     * Debug & performance *
     * =================== */

    private debugHandler?: DebugHandler;
}
