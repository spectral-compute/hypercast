import {EventEmitter} from "events";

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
    }

    add(path: string): ServerFile {
        const sf = new ServerFile();
        this.files.set(path, sf);
        this.emit('add', path);
        return sf;
    }

    remove(path: string): void {
        this.files.delete(path);
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
