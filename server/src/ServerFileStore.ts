import {EventEmitter} from "events";
import {Logger} from "./Log";

const log = new Logger('Server File Store');

class InterleavedFile {
    constructor(fileStore: ServerFileStore, namePattern: string, captureGroups: string[], interleaveTotal: number) {
        this.fileStore = fileStore;
        this.captureGroups = captureGroups;
        this.interleaveTotal = interleaveTotal;

        /* Figure out what the path for the interleaved file is. */
        let substitutedPattern: string = namePattern;
        captureGroups.forEach((value: string, index: number): void => {
            substitutedPattern = substitutedPattern.replaceAll('{' + (index + 1) + '}', value);
        });
        this.interleavedPath = substitutedPattern;

        /* Create the file. */
        this.serverFile = fileStore.add(this.interleavedPath);
        log.debug(`Creating interleaved file ${this.interleavedPath}`);
    }

    captureGroupsMatch(captureGroups: string[]): boolean {
        if (captureGroups.length != this.captureGroups.length) {
            return false;
        }

        let result = true;
        captureGroups.forEach((value: string, index: number): void => {
            if (value != this.captureGroups[index]) {
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
        this.eventHandlers.set(path, new Array<[string, any, any]>());
        const eventHandlersForPath = this.eventHandlers.get(path)!;

        /* Set up the handler for when the source file is deleted. */
        const onRemove = (removedPath: string): void => {
            if (removedPath == path) {
                this.onSourceDeleted(path);
                return;
            }
        };
        this.fileStore.on('remove', onRemove);
        eventHandlersForPath.push(['remove', onRemove, this.fileStore]);

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
            if (b.length == 0) {
                return; // We use 0-lengthed chunks to mean "end of file".
            }
            this.addChunk(index, b);
        };
        sf.on('add', onAdd);
        eventHandlersForPath.push(['add', onAdd, sf]);

        /* Handle the finishing of the source file. */
        const onFinish = (): void => {
            this.addChunk(index, Buffer.alloc(0));
            this.onSourceFinish(path);
        };
        sf.on('finish', onFinish);
        eventHandlersForPath.push(['finish', onFinish, sf]);
    }

    private addChunk(index: number, b: Buffer): void {
        const getValueAsBuffer = (value: number, length: number): Buffer => {
            const b = Buffer.alloc(length);
            b.writeUIntLE(value, 0, length);
            return b;
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

        this.serverFile.add(Buffer.concat([getValueAsBuffer(contentId, 1), lengthBuffer, b]));
    }

    private onSourceFinish(path: string) {
        this.sourceInProgressPaths.delete(path);
        if (this.interleaveCount == this.interleaveTotal && this.sourceInProgressPaths.size == 0) {
            this.serverFile.finish();
            log.debug(`Finishing interleaved file ${this.interleavedPath}`);
        }
    }

    private onSourceDeleted(path: string) {
        for (const entry of this.eventHandlers.get(path)!) {
            entry[2].off(entry[0], entry[1]);
        }
        this.eventHandlers.delete(path);

        if (this.interleaveCount == this.interleaveTotal && this.eventHandlers.size == 0) {
            this.fileStore.remove(this.interleavedPath);
            log.debug(`Removing interleaved file ${this.interleavedPath}`);
        }
    }

    private readonly fileStore: ServerFileStore;
    private readonly captureGroups: string[];
    private readonly interleaveTotal: number; // The number of files to interleave.

    private interleaveCount: number = 0; // The number of files interleaved so far.
    private readonly interleavedPath: string; // The name of the interleaved file.
    private readonly serverFile: ServerFile; // The interleaved file.
    private readonly sourceInProgressPaths = new Set<string>(); // All source paths not yet finished.

    // Map from source path that has not been deleted to [event name, event handler, event emitter]. Needed so we can
    // remove the event handlers and keep track of which sources have been deleted.
    private readonly eventHandlers = new Map<string, [string, any, any][]>();
}

class InterleavePattern {
    constructor(fileStore: ServerFileStore, namePattern: string, patterns: RegExp[]) {
        log.debug(`Creating interleave pattern: ${namePattern}`);
        for (const pattern of patterns) {
            log.debug(`    "${pattern.source}"`);
        }

        this.fileStore = fileStore;
        this.namePattern = namePattern;
        this.patterns = patterns;

        fileStore.on('add', (path: string): void => {
            this.onServerAddPath(path);
        });
    }

    private onServerAddPath(path: string) {
        for (let patternIndex = 0; patternIndex < this.patterns.length; patternIndex++) {
            const match = this.patterns[patternIndex]!.exec(path);
            if (!match) {
                continue;
            }
            this.onServerAddMatchingPath(path, patternIndex, match.slice(1));
        }
    }

    private onServerAddMatchingPath(path: string, patternIndex: number, captureGroups: string[]) {
        for (const interleavedFile of this.interleavedFiles) {
            if (interleavedFile.captureGroupsMatch(captureGroups)) {
                interleavedFile.addFileAtIndex(path, patternIndex);
                return;
            }
        }
        const interleavedFile = new InterleavedFile(this.fileStore, this.namePattern, captureGroups,
                                                    this.patterns.length);
        interleavedFile.addFileAtIndex(path, patternIndex);
        this.interleavedFiles.push(interleavedFile);
    }

    private readonly fileStore: ServerFileStore;
    private readonly namePattern: string;
    private readonly patterns: RegExp[];
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
        this.emit('add', b);
    }

    finish(): void {
        /* Notify everything that this file is now complete. */
        this.finished = true;
        this.emit('finish');
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
     * Each file has a corresponding index (according to the patterns array). The contents of the source files are
     * inserted into the interleaved file in chunks. The format is simply a repetition of:
     *  - An 8-bit content ID. The low 6 bits are the file index. The high 2 bits are the width of the length:
     *     - 0: 8-bit length.
     *     - 1: 16-bit length.
     *     - 2: 32-bit length.
     *     - 3: 64-bit length.
     *  - The N-bit little-endian length.
     *  - The data chunk.
     *
     * @param namePattern The pattern to use to construct the path for the interleaved file. {n} is replaced with
     *                    capture group n.
     * @param patterns A list of patterns to interleave. The capture groups will be used as a key so that only files
     *                 with matching capture groups are interleaved. Therefore, each pattern must have the same number
     *                 of capture groups.
     */
    addInterleavingPattern(namePattern: string, patterns: RegExp[]): void {
        new InterleavePattern(this, namePattern, patterns);
    }

    add(path: string): ServerFile {
        const sf = new ServerFile();
        this.files.set(path, sf);
        this.emit('add', path);
        return sf;
    }

    remove(path: string): void {
        this.files.delete(path);
        this.emit('remove', path);
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
