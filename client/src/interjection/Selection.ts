import {waitForPts} from "../Util";
import {abortablePromise, API, waitForEvent} from "live-video-streamer-common";

export class Interjection
{
    /**
     * Constructor :)
     *
     * @param index The index of the interjection within the interjection selection.
     * @param metadata The metadata for the interjection.
     * @param mediaElement The media element that the interjection is playing in.
     * @param signal The signal that is used to abort playback.
     * @param startPts The PTS of the start of the interjection in the media element.
     */
    constructor(public readonly index: number, public readonly metadata: API.Interjection,
                private readonly mediaElement: HTMLMediaElement, private readonly signal: AbortSignal,
                private readonly startPts: number)
    {
    }

    /**
     * Get the PTS since the start of the interjection.
     *
     * @param mediaElement The media element that the interjection is playing in.
     */
    getPts(): number {
        return this.mediaElement.currentTime - this.startPts;
    }

    /**
     * Wait for the interjection to reach the requested PTS.
     *
     * This method can return *slightly* early.
     *
     * @param pts The presentation timestamp within the interjection to wait for. This PTS is adjusted according to the
     *            PTS of the start of the interjection.
     */
    waitForPts(pts: number): Promise<void> {
        return waitForPts(pts + this.startPts, this.mediaElement, this.signal);
    }

    /**
     * Wait for the end of the interjection.
     */
    waitForEnd(): Promise<void> {
        // Wait until either the PTS reaches the end of the interjection, or the underlying media tag ends. This latter
        // condition accounts for the fact that the PTS may not reach exactly the advertized duration, e.g: because the
        // audio and video are slightly different lengths.
        return Promise.race([this.waitForPts(this.metadata.contentDuration / 1000),
                             waitForEvent("ended", this.mediaElement, this.signal)]);
    }
}

/**
 * Keeps track of a selected list of interjections, and what their playback state is.
 */
export class InterjectionSelection {
    constructor(public readonly selection: string[],
                public readonly metadata: API.InterjectionSet,
                private readonly mediaElement: HTMLMediaElement, private readonly signal: AbortSignal)
    {
    }

    /**
     * Start the next interjection and wait for it to start playing.
     */
    async startNextInterjection(index: number, metadata: API.Interjection, startPts: Promise<number>): Promise<void> {
        const nextInterjection = new Interjection(index, metadata, this.mediaElement, this.signal, await startPts);
        await nextInterjection.waitForPts(0);
        this.currentInterjection = nextInterjection;
        this.notifyAll();
    }

    /**
     * Wait for an important event, such as a change of interjection.
     */
    wait(): Promise<void> {
        return abortablePromise((resolve): void => {
            this.promises.push(resolve);
        }, this.signal);
    }

    /**
     * Wait for the video with the given index to start playing.
     */
    async waitForInterjectionIndex(index: number): Promise<Interjection> {
        while ((this.currentInterjection?.index ?? -1) < index) {
            await this.wait();
        }
        return this.currentInterjection!;
    }

    /**
     * Wait for the last interjection to start playing.
     */
    waitForLastInterjection(): Promise<Interjection> {
        return this.waitForInterjectionIndex(this.selection.length - 1);
    }

    /**
     * Notify everything awaiting a call to wait.
     */
    private notifyAll(): void {
        for (const fulfill of this.promises) {
            fulfill();
        }
        this.promises = new Array();
    }

    /**
     * The currently playing interjection.
     */
    private currentInterjection: Interjection | null = null;

    /**
     * A list of promises created by wait that need to be resolved by notifyAll.
     *
     * Each element is a pair: resolve and reject.
     */
    private promises: Array<() => void> = new Array();
}
