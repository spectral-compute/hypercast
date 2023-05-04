import {MseWrapper} from "./MseWrapper";
import {Interjection, InterjectionSelection} from "./Selection";
import {waitForPts} from "../Util"
import {API, sleep} from "live-video-streamer-common";
import {assertType} from "@ckitching/typescript-is";

/**
 * Information about the interjection(s) to request to give to the client library user when asking it to select
 * interjections.
 */
export interface InterjectionRequest {
    /**
     * The minimum length of the remaining interjection request time, in ms.
     *
     * If interjection(s) are chosen that is shorter than this, the interjection selection function might be called
     * again for more interjections to play after the ones already returned.
     */
    minLength: number;

    /**
     * The maximum length of the remaining interjection request time, in ms.
     */
    maxLength: number;

    /**
     * An object given by the caller of the API on the server.
     */
    other?: any;
}

/**
 * Controls the player's response to the Interjection API.
 */
export class InterjectionPlayer {
    /**
     * Start showing interjections.
     *
     * This is intended to support different interjection selection methods, including geographical selection, selection
     * by advert auction, and client-side selection.
     *
     * @param request The interjection request to try to play interjections for.
     * @param mainVideoElement The video element that contains the main stream.
     * @param interjectionVideoElement The video element to play the interjection in.
     * @param startMainStream Starts the playback of the main stream. This does not unhide or unmute its video tag.
     * @param stopMainStream Stops the playback of the main stream. This does not hide or mute its video tag.
     * @param onSelectInterjection Choose some interjections to play. The result is (a promise for) the list of IDs from
     *                             the interjections map in the interjection set in the order to play them. The
     *                             interjection set is filtered to include only those interjections that can be played
     *                             in conformance with the request. This function is intended to come from the user of
     *                             the client library. It can be called multiple times if it did not return enough
     *                             interjections to fill the entire requested range the first time. If this argument is
     *                             not given, then one interjection is chosen at random.
     * @param onInterjectionEvent This is called for each interjection that is returned by onSelectInterjection for each
     *                            applicable event. The "select" event indicates that an interjection was selected. The
     *                            "start" event indicates that the interjection started playing. The "end" event
     *                            indicates that the interjection finished playback successfully. The "interrupt" event
     *                            indicates that the interjection stopped playback after it started but before it
     *                            finished. The "cancel" event indicates that the interjection was selected but did not
     *                            start playback. The interjection set is the same as the one given to
     *                            onSelectInterjection. This function is intended to come from the user of the client
     *                            library.
     */
    constructor(private readonly request: API.InterjectionRequest,
                private readonly mainVideoElement: HTMLVideoElement,
                private readonly interjectionVideoElement: HTMLVideoElement,
                private readonly startMainStream: () => void,
                private readonly stopMainStream: () => void,
                private readonly onSelectInterjection: ((signal: AbortSignal,
                                                         request: InterjectionRequest,
                                                         interjectionSet: API.InterjectionSet,
                                                         previousSelected: string[]) => Promise<string[]>) | null,
                private readonly onInterjectionEvent: ((event: "select" | "start" | "end" | "interrupt" | "cancel",
                                                        request: InterjectionRequest,
                                                        interjectionId: string,
                                                        interjectionSet: API.InterjectionSet,
                                                        interjection: API.Interjection) => void) | null,
                private readonly onError: (description: string) => void) {
        void this.onInterjectionEvent;

        this.mseWrapper = new MseWrapper(this.interjectionVideoElement, this.aborter.signal,
                                         (description) => this.onError(description),
                                         (fn) => this.spawn(fn),
                                         (url, type, what) => this.download(url, type, what));

        /* Set up throttling. */
        // Throttling to keep the live stream smooth at the start, but remember that the first frame is an I frame, and
        // thus significantly larger than the rest. The interjections might also have B frames, so extra buffer is
        // needed to even play properly. Since we might only get a second to start the download, compromise and download
        // a bit faster than real-time. The numerator sets how much to download in bytes at the average bitrate of the
        // video (so less in practice, because of the I frame) before playback begins.
        this.mseWrapper.setThrottling(2 / this.request.minSelectTime);

        /* Start the interjections asynchronously. */
        this.spawn(async (): Promise<void> => {
            // Download the interjection set metadata.
            const j = await this.download(`${this.request.setUrl}/interjection-set.json`, "json", "set metadata");
            assertType<API.InterjectionSet>(j);
            this.interjectionSet = j;

            // Select and show the first selection of interjections.
            await this.selectAndShowInterjections(null);
        });

        /* Handle the case where we have to return to the main stream while playing an interjection. */
        if (request.playingInterjectionPriorityTime === null) {
            return;
        }

        this.spawn(async (): Promise<void> => {
            // Wait for it to be time to start the main stream back up.
            await sleep(this.request.maxPlayEndPts - this.getPts() * 1000 +
                        this.request.playingInterjectionPriorityTime! - this.request.mainStreamWarmUpTime,
                        this.aborter.signal);

            // If we're already finishing, don't do so again.
            if (this.finishing) {
                return;
            }

            // We're now finishing, even if we never really started.
            this.finishing = true;

            // If we never switched away from the main stream, keep it that way.
            if (this.mainStreamSuspendTime !== null) {
                // Switch back to the main stream.
                await this.switchToMainStream(null);
            }

            // Abort anything that's currently happening and report on interruptions/cancellations.
            this.stop();
        });
    }

    /**
     * Stop showing the interjection.
     */
    stop(): void {
        this.finishing = true;
        this.aborter.abort(); // Stop anything that's downloading, sleeping, and so on.
        // TODO: onInterjectionEvent "interrupt" and "cancel". Maybe by catching the AbortError elsewhere.
    }

    /**
     * Run one iteration of selecting interjections and showing them, and (by recursion) schedule the next.
     */
    private async selectAndShowInterjections(previousSelection: InterjectionSelection | null): Promise<void> {
        /* Select some interjections. */
        const selection: InterjectionSelection | null = await this.selectInterjections(previousSelection);
        if (selection === null) {
            // TODO: Fallback.
            this.mseWrapper.end();
            await this.switchToMainStream(await previousSelection?.waitForLastInterjection() ?? null);
            return;
        }

        /* Spawn the next interjection selection (it'll wait for the already selected interjections to be mostly
           over). */
        this.spawn(((selection) => () => this.selectAndShowInterjections(selection))(selection));

        /* Play each interjection in turn. */
        this.spawn(((selection) => () => this.showInterjection(selection, 0))(selection));
    }

    /**
     * Select the next interjections.
     *
     * @param previousSelection The previous selection object that keeps track of what's currently playing.
     * @return The array of interjections to display next and the filtered interjection set from which they were chosen.
     *         Null if there are no interjections.
     */
    private async selectInterjections(previousSelection: InterjectionSelection | null): Promise<InterjectionSelection | null> {
        /* Figure out when the interjection selection should happen and wait, and figure out when the interjection would
           start playing, as a main stream PTS. */
        let interjectionEstimatedStartPts: number = 0;

        // We're waiting for the right time in the main stream.
        if (previousSelection === null) {
            // Wait for the earliest selection time.
            await waitForPts((this.request.minPlayStartPts - this.request.maxSelectTime) / 1000, this.mainVideoElement,
                             this.aborter.signal);

            // Figure out when the interjection would start playing.
            interjectionEstimatedStartPts =
                Math.max(this.mainVideoElement.currentTime * 1000 + this.request.minSelectTime,
                         this.request.minPlayStartPts);

            // Make sure we haven't exceeded the minimum selection time.
            if (interjectionEstimatedStartPts > this.request.maxPlayStartPts) {
                return null; // We're too late to start interjections.
            }
        }

        // We're waiting for the right time in the last interjection.
        else {
            // Wait for the last interjection to start playing. Note: The selection length is always at least 1.
            const interjection: Interjection = await previousSelection.waitForLastInterjection();

            // Wait for the right time in this video.
            await interjection.waitForPts((interjection.metadata.contentDuration - this.request.maxSelectTime) / 1000);

            // Make sure there's enough time before the end of the previous interjection to select another.
            if (interjection.getPts() * 1000 > interjection.metadata.contentDuration - this.request.minSelectTime) {
                return null; // We're too late to request another interjection.
            }

            // Figure out when the interjection would start playing.
            interjectionEstimatedStartPts =
                this.getPts() * 1000 + (interjection.metadata.contentDuration - interjection.getPts() * 1000);
        }

        /* Figure out the contents of the interjection request information. */
        const request: InterjectionRequest = { minLength: this.request.minPlayEndPts - interjectionEstimatedStartPts,
                                               maxLength: this.request.maxPlayEndPts - interjectionEstimatedStartPts,
                                               other: this.request.other };

        /* Figure out what interjections are allowed. */
        const interjectionSet: API.InterjectionSet = this.getFilteredInterjectionSet(request.maxLength);
        if (Object.keys(interjectionSet.interjections).length === 0) {
            return null; // There are no interjections to choose from.
        }

        /* Make sure we're not finishing interjections, and thus shouldn't select any more. */
        if (this.finishing) {
            return null;
        }

        /* Select the next interjections. */
        // TODO: Use a separate aborter for pausing the video so we can cancel the selection on pause, and resume it on
        //       unpause.
        const interjectionSelection: string[] =
            // If we've been given a selection function, use it.
            await this.onSelectInterjection?.(this.aborter.signal, request, interjectionSet,
                                              this.previousSelectedInterjections) ??

            // Otherwise, choose randomly.
            this.selectRandomInterjection(interjectionSet);

        /* Record the selected interjections. */
        this.previousSelectedInterjections = this.previousSelectedInterjections.concat(interjectionSelection);

        /* Done :) Null for "no interjections" as above. */
        return (interjectionSelection.length === 0) ? null :
               new InterjectionSelection(interjectionSelection, interjectionSet, this.interjectionVideoElement,
                                         this.aborter.signal);
    }

    /**
     * Select a random interjection that hasn't previously been shown.
     *
     * @param interjectionSet The filtered interjection set.
     */
    private selectRandomInterjection(interjectionSet: API.InterjectionSet): string[] {
        /* Build a list of interjections that haven't been shown. */
        const unselectedInterjections: string[] = new Array();
        for (const name of Object.keys(interjectionSet.interjections)) {
            if (this.previousSelectedInterjections.indexOf(name) < 0) {
                unselectedInterjections.push(name);
            }
        }

        /* Return empty if there are no such interjections. */
        if (unselectedInterjections.length === 0) {
            return [];
        }

        /* Choose an interjection at random. */
        return [unselectedInterjections[Math.floor(Math.random() * unselectedInterjections.length)]!];
    }

    /**
     * Show the interjection in currentInterjection.
     */
    private async showInterjection(selection: InterjectionSelection, index: number): Promise<void> {
        // TODO: interjectionSelectionPriorityTime

        const name: string = selection.selection[index]!;

        /* Download the interjection metadata. */
        const interjectionMetadata = await this.download(`${this.request.setUrl}/${name}/interjection.json`, "json",
                                                         "metadata");
        assertType<API.Interjection>(interjectionMetadata);

        /* Enqueue the downloading of the interjection's streams. */
        const numSegments = Math.ceil(interjectionMetadata.contentDuration / interjectionMetadata.segmentDuration);
        const startPts: Promise<number> =
            this.mseWrapper.append(`${this.request.setUrl}/${name}`, numSegments, interjectionMetadata.segmentDuration,
                                   interjectionMetadata.video, interjectionMetadata.audio);

        /* Swap from the main stream tag to the interjection video tag. */
        await this.switchToInterjections();

        /* Track and wait for the start of this interjection. */
        await selection.startNextInterjection(index, interjectionMetadata, startPts);

        /* Spawn for the following interjection. */
        if (index === selection.selection.length - 1) {
            return;
        }
        this.spawn(((selection, index) => () => this.showInterjection(selection, index + 1))(selection, index));
    }

    /**
     * Filter the interjection set to those interjections that can be played now.
     *
     * @param maxLength The maximum length of interjection that can be played.
     */
    private getFilteredInterjectionSet(maxLength: number): API.InterjectionSet {
        const interjections: { [key: string]: API.InterjectionSetInterjection } = {};
        for (const [key, value] of
             Object.entries<API.InterjectionSetInterjection>(this.interjectionSet!.interjections))
        {
            if (value.duration > maxLength) {
                continue;
            }
            interjections[key] = value;
        }
        return { interjections: interjections, other: this.interjectionSet!.other };
    }

    /**
     * Switch from the live stream to the interjection video.
     */
    private async switchToInterjections(): Promise<void> {
        /* If we're already on the interjections, don't switch again. */
        if (this.started) {
            return;
        }
        this.started = true;

        /* Wait for it to be time to start showing interjections in general. */
        await waitForPts(this.request.minPlayStartPts / 1000, this.mainVideoElement, this.aborter.signal);

        /* Switch to the interjections. */
        const muted = this.mainVideoElement.muted;
        this.mainStreamSuspendTime = performance.now();
        this.mainStreamSuspendPts = this.mainVideoElement.currentTime;

        this.mainVideoElement.style.display = "none";
        this.mainVideoElement.muted = true;
        this.stopMainStream();
        this.mseWrapper.setThrottling(null); // Throttling is no longer needed.

        this.interjectionVideoElement.style.display = "";
        this.interjectionVideoElement.muted = muted;
        this.interjectionVideoElement.play();
    }

    /**
     * Switch from the interjections to the main stream.
     *
     * @param after Switch to the main stream after this interjection.
     */
    private async switchToMainStream(after: Interjection | null): Promise<void> {
        /* If we're already on the main stream, don't switch again. */
        if (this.finishing) {
            return;
        }
        this.finishing = true;

        /* Wait for it to be time to start the main stream. */
        // TODO: Get rid of main stream warmup time, and instead download from when the segment we'll need becomes
        //       pre-available. We don't lose anything by doing that except perhaps occasionally having to download an
        //       extra segment.
        if (after !== null) {
            await after.waitForPts((after.metadata.contentDuration - this.request.mainStreamWarmUpTime) / 1000);
        }

        /* Start the main stream. */
        this.startMainStream();

        /* Wait for it to be time to switch to the main stream. */
        if (after === null) {
            sleep(this.request.mainStreamWarmUpTime);
        } else {
            await after.waitForEnd();
        }

        /* Switch to the main stream. */
        const muted = this.interjectionVideoElement.muted;
        this.mainStreamSuspendTime = null
        this.mainStreamSuspendPts = null

        this.interjectionVideoElement.style.display = "none";
        this.interjectionVideoElement.muted = true;
        this.interjectionVideoElement.pause();

        this.mainVideoElement.style.display = "";
        this.mainVideoElement.muted = muted;
    }

    /**
     * Get the (actual or predicted) timestamp of the main stream in s.
     */
    private getPts(): number {
        return (this.mainStreamSuspendTime === null) ?

               // The main stream is not suspended, so use its PTS.
               this.mainVideoElement.currentTime :

               // The main stream is suspended, so figure out what its PTS would be based on the current time, the time
               // of suspension, and the PTS at that time.
               ((performance.now() - this.mainStreamSuspendTime) / 1000.0) + this.mainStreamSuspendPts!;
    }

    /**
     * Download a complete URL.
     *
     * @param url The URL to download.
     * @param type What type to download as.
     * @param what A description of what is being downloaded for inclusion in error messages. This should be a
     *             noun-phrase that can follow the word "interjection".
     * @return The downloaded object.
     */
    private async download(url: string, type: "binary" | "json" | "string", what: string): Promise<any> {
        try {
            const response: Response = await fetch(url, { signal: this.aborter.signal });
            if (response.status !== 200) {
                throw new Error(`got HTTP status code ${response.status}`);
            }
            switch (type) {
                case "binary": return await response.arrayBuffer();
                case "json": return await response.json();
                case "string": return await response.text();
            }
        } catch (ex: any) {
            const e = ex as Error;
            if (e.name === 'AbortError') {
                throw e;
            }
            throw Error(`Error downloading interjection ${what}: ${e.message}.`);
        }
    }

    /**
     * Spawn a coroutine.
     *
     * @param fn The function to run in the coroutine. Can be asynchronous or not, but must take no arguments and return
     *           nothing.
     */
    private spawn(fn: () => void | Promise<void>) {
        /* Wrap the function in something to check the aborter and handle exceptions. */
        void ((fn) => async (): Promise<void> => {
            try {
                await fn();
            } catch (ex: any) {
                const e = ex as Error;
                if (e.name === 'AbortError') {
                    return;
                }
                this.onError(e.message);
            }
        })(fn)();
    }

    /**
     * Used to stop downloads.
     */
    private aborter: AbortController = new AbortController();

    /**
     * Uses Media Source Extensions to play interjections.
     */
    private mseWrapper: MseWrapper;

    /**
     * The interjection set metadata that describes the available interjections, and what to do with them.
     */
    private interjectionSet: API.InterjectionSet | null = null;

    /**
     * The list of interjections that have already been selected.
     */
    private previousSelectedInterjections: string[] = new Array<string>();

    /**
     * The time (in ms) that the main stream was suspended.
     */
    private mainStreamSuspendTime: number | null = null;

    /**
     * The presentation timestamp (in s) that the main stream was suspended.
     */
    private mainStreamSuspendPts: number | null = null;

    /**
     * Set when the interjections first start playing.
     */
    private started: boolean = false;

    /**
     * Set to true when the interjections are finishing so that no new interjections are selected.
     */
    private finishing: boolean = false;
}
