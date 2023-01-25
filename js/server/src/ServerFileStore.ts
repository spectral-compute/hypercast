import {EventEmitter} from "events";
import {Logger} from "./Log";
import {MinimumInterleaveRate} from "./MinimumInterleaveRate";

const log = new Logger("Server File Store");

class InterleavedFile {
    constructor(fileStore: ServerFileStore, namePattern: string, captureGroups: string[], interleaveTotal: number,
                timestampInterval: number, minimumRate: number, minimumRateWindowSize: number) {
        this.fileStore = fileStore;
        this.captureGroups = captureGroups;
        this.interleaveTotal = interleaveTotal;
        this.timestampInterval = timestampInterval;

        /* Make sure the minimum rate is maintained. */
        if (minimumRate > 0) {
            this.minimumRateTracker = new MinimumInterleaveRate((b: Buffer): void => {
                // The padding data file index is here. If this is changed, then change the documentation and the call
                // to this.onData() in Deinterleaver.acceptData() in the client too.
                this.addChunk(31, b);
            }, minimumRate, minimumRateWindowSize);
        }

        /* Figure out what the path for the interleaved file is. */
        let substitutedPattern: string = namePattern;
        captureGroups.forEach((value: string, index: number): void => {
            substitutedPattern = substitutedPattern.replaceAll(`{${index + 1}}`, value);
        });
        this.interleavedPath = substitutedPattern;

        /* Create the file. */
        this.serverFile = fileStore.add(this.interleavedPath);
        log.debug(`Creating interleaved file ${this.interleavedPath}`);
    }

    captureGroupsMatch(captureGroups: string[]): boolean {
        if (captureGroups.length !== this.captureGroups.length) {
            return false;
        }

        let result = true;
        captureGroups.forEach((value: string, index: number): void => {
            if (value !== this.captureGroups[index]) {
                result = false;
            }
        });
        return result;
    }

    addFileAtIndex(path: string, index: number): void {
        log.debug(`Adding ${path} to interleaved file ${this.interleavedPath}`);

        /* By definition, this path exists. */
        this.interleaveCount++;
        const sf: ServerFile = this.fileStore.get(path);
        this.sourceInProgressPaths.add(path);
        this.eventHandlers.set(path, new Array<[string, (...args: any[]) => void, ServerFileStore]>());
        const eventHandlersForPath = this.eventHandlers.get(path)!;

        /* Set up the handler for when the source file is deleted. */
        const onRemove = (removedPath: string): void => {
            if (removedPath === path) {
                this.onSourceDeleted(path);
            }
        };
        this.fileStore.on("remove", onRemove);
        eventHandlersForPath.push(["remove", onRemove, this.fileStore]);

        /* Write the initial contents of the buffer. */
        sf.writeWith((b: Buffer): void => {
            this.addChunk(index, b);
        });

        /* Handle in-progress-ness. */
        if (sf.isFinished()) {
            this.onSourceFinish(path);
            return;
        }

        /* Handle new data as it comes in. */
        const onAdd = (b: Buffer): void => {
            if (b.length === 0) {
                return; // We use 0-lengthed chunks to mean "end of file".
            }
            this.addChunk(index, b);
        };
        sf.on("add", onAdd);
        eventHandlersForPath.push(["add", onAdd, sf]);

        /* Handle the finishing of the source file. */
        const onFinish = (): void => {
            this.addChunk(index, Buffer.alloc(0));
            this.onSourceFinish(path);
        };
        sf.on("finish", onFinish);
        eventHandlersForPath.push(["finish", onFinish, sf]);
    }

    private addChunk(index: number, b: Buffer): void {
        const getValueAsBuffer = (value: number, length: number): Buffer => {
            const vb = Buffer.alloc(length);
            for (let i = 0; i < length; i += 4) {
                vb.writeUIntLE(Math.floor(value) % (2 ** 32), i, Math.min(length - i, 4));
                value = Math.floor(value / (2 ** 32));
            }
            return vb;
        };

        let contentId: number;
        let lengthBuffer: Buffer;
        if (b.length < 256) {
            contentId = index;
            lengthBuffer = getValueAsBuffer(b.length, 1);
        } else if (b.length < 65536) {
            contentId = index + 64;
            lengthBuffer = getValueAsBuffer(b.length, 2);
        } else if (b.length < 4294967296) {
            contentId = index + 2 * 64;
            lengthBuffer = getValueAsBuffer(b.length, 4);
        } else {
            contentId = index + 3 * 64;
            lengthBuffer = getValueAsBuffer(b.length, 8);
        }

        const now: number = Date.now();
        if (this.startTimestamp === 0) {
            this.startTimestamp = now;
        }

        let timestampBuffer: Buffer;
        if (now - this.startTimestamp >= this.timestampCount * this.timestampInterval && b.length !== 0) {
            this.timestampCount++;
            timestampBuffer = getValueAsBuffer(Math.round(now * 1000), 8); // Timestamp in µs.
            contentId += 32;
        } else {
            timestampBuffer = Buffer.alloc(0);
        }

        const chunk = Buffer.concat([getValueAsBuffer(contentId, 1), lengthBuffer, timestampBuffer, b]);
        this.serverFile.add(chunk);

        /* Keep the minimum rate tracker up to date. */
        if (this.minimumRateTracker === null) {
            return;
        }
        if (!this.minimumRateTracker.isRunning()) {
            // This is started here so that if anything ever adds an interleave during pre-availability, it doesn't
            // generate a minimum bitrate at that point. Warmup data should be handled separately if we want that.
            this.minimumRateTracker.start();
        }
        this.minimumRateTracker.addData(chunk.length);
    }

    private onSourceFinish(path: string): void {
        this.sourceInProgressPaths.delete(path);
        if (this.interleaveCount === this.interleaveTotal && this.sourceInProgressPaths.size === 0) {
            if (this.minimumRateTracker?.isRunning()) {
                this.minimumRateTracker.stop();
            }
            this.serverFile.finish();
            log.debug(`Finishing interleaved file ${this.interleavedPath}`);
        }
    }

    private onSourceDeleted(path: string): void {
        for (const entry of this.eventHandlers.get(path)!) {
            entry[2].off(entry[0], entry[1]);
        }
        this.eventHandlers.delete(path);

        if (this.interleaveCount === this.interleaveTotal && this.eventHandlers.size === 0) {
            this.fileStore.remove(this.interleavedPath);
            log.debug(`Removing interleaved file ${this.interleavedPath}`);
        }
    }

    private readonly fileStore: ServerFileStore;
    private readonly minimumRateTracker: MinimumInterleaveRate | null = null;
    private readonly captureGroups: string[];
    private readonly interleaveTotal: number; // The number of files to interleave.
    private readonly timestampInterval: number; // Period between timestamps in the interleave.
    private startTimestamp: number = 0; // The time of the first chunk in the interleave. Zero if none.
    private timestampCount: number = 0; // The number of timestamps inserted into the interleave.

    private interleaveCount: number = 0; // The number of files interleaved so far.
    private readonly interleavedPath: string; // The name of the interleaved file.
    private readonly serverFile: ServerFile; // The interleaved file.
    private readonly sourceInProgressPaths = new Set<string>(); // All source paths not yet finished.

    // Map from source path that has not been deleted to [event name, event handler, event emitter]. Needed so we can
    // remove the event handlers and keep track of which sources have been deleted.
    private readonly eventHandlers =
        new Map<string, [string, (...args: any[]) => void, ServerFileStore | ServerFile][]>();
}

class InterleavePattern {
    constructor(fileStore: ServerFileStore, namePattern: string, patterns: RegExp[], timestampInterval: number,
                minimumRate: number, minimumRateWindowSize: number) {
        log.debug(`Creating interleave pattern: ${namePattern}`);
        for (const pattern of patterns) {
            log.debug(`    "${pattern.source}"`);
        }

        this.fileStore = fileStore;
        this.namePattern = namePattern;
        this.patterns = patterns;
        this.timestampInterval = timestampInterval;
        this.minimumRate = minimumRate;
        this.minimumRateWindowSize = minimumRateWindowSize;

        fileStore.on("add", (path: string): void => {
            this.onServerAddPath(path);
        });
    }

    private onServerAddPath(path: string): void {
        for (let patternIndex = 0; patternIndex < this.patterns.length; patternIndex++) {
            const match = this.patterns[patternIndex]!.exec(path);
            if (!match) {
                continue;
            }
            this.onServerAddMatchingPath(path, patternIndex, match.slice(1));
        }
    }

    private onServerAddMatchingPath(path: string, patternIndex: number, captureGroups: string[]): void {
        for (const interleavedFile of this.interleavedFiles) {
            if (interleavedFile.captureGroupsMatch(captureGroups)) {
                interleavedFile.addFileAtIndex(path, patternIndex);
                return;
            }
        }
        const interleavedFile = new InterleavedFile(this.fileStore, this.namePattern, captureGroups,
                                                    this.patterns.length, this.timestampInterval,
                                                    this.minimumRate, this.minimumRateWindowSize);
        interleavedFile.addFileAtIndex(path, patternIndex);
        this.interleavedFiles.push(interleavedFile);
    }

    private readonly fileStore: ServerFileStore;
    private readonly namePattern: string;
    private readonly patterns: RegExp[];
    private readonly timestampInterval: number;
    private readonly minimumRate: number;
    private readonly minimumRateWindowSize: number;
    private readonly interleavedFiles = new Array<InterleavedFile>();
}

/**
 * A logical file being hosted by this server. It's actually resident in memory, since all files handled by this
 * server are transient anyway. They get created by ffmpeg, HTTP `PUT`'d to this server, held in memory and served,
 * and then deleted.
 *
 * A completely different server, running somewhere else, is responsible for watching and recording the video streams.
 * This approach means that archive access doesn't compete for the (scarce) bandwidth at the casino site, and allows
 * us to distribute the archiving as widely as we fancy.
 */
class ServerFile extends EventEmitter {
    private readonly buffers: Buffer[] = [];
    private finished: boolean = false;

    constructor() {
        super();
        this.setMaxListeners(256);
    }

    add(b: Buffer): void {
        this.buffers.push(b);
        this.emit("add", b);
    }

    finish(): void {
        /* Notify everything that this file is now complete. */
        this.finished = true;
        this.emit("finish");
    }

    isFinished(): boolean {
        return this.finished;
    }

    writeWith(fn: (b: Buffer) => void): void {
        this.buffers.forEach((b: Buffer) => {
            fn(b);
        });
    }
}

/**
 * The in-memory cache from which GET requests are satisfied.
 */
export class ServerFileStore extends EventEmitter {
    constructor() {
        super();
        this.setMaxListeners(256);
    }

    /**
     * Make the file server automatically generate interleaved files from a list of other files.
     *
     * The interleaved files are useful to work around the fact that some buffering in the network happens with a fixed
     * buffer size. This means files that stream slowly (e.g: audio) get delayed. By interleaving them with higher
     * bandwidth files (e.g: video), the latter evicts the buffer for the former.
     *
     * @param namePattern The pattern to use to construct the path for the interleaved file. {n} is replaced with
     *                    capture group n.
     * @param patterns A list of patterns to interleave. The capture groups will be used as a key so that only files
     *                 with matching capture groups are interleaved. Therefore, each pattern must have the same number
     *                 of capture groups.
     * @param timestampInterval Average period between timestamps in the interleave.
     * @param minimumRate The minimum rate to maintain, in bytes per second, for this interleave.
     * @param minimumRateWindowSize The window over which to evaluate the minimum rate, in ms.
     */
    addInterleavingPattern(namePattern: string, patterns: RegExp[], timestampInterval: number, minimumRate: number,
                           minimumRateWindowSize: number): void {
        new InterleavePattern(this, namePattern, patterns, timestampInterval, minimumRate, minimumRateWindowSize);
    }

    add(path: string): ServerFile {
        const sf = new ServerFile();
        this.files.set(path, sf);
        this.emit("add", path);
        return sf;
    }

    remove(path: string): void {
        this.files.delete(path);
        this.emit("remove", path);
    }

    has(path: string): boolean {
        return this.files.has(path);
    }

    get(path: string): ServerFile {
        return this.files.get(path)!;
    }

    // Map from file to ServerFile.
    private readonly files = new Map<string, ServerFile>();
}
