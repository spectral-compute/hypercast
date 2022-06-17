import {readFile, stat} from "fs/promises";

import {Liveness} from "./VideoServer";
import {Logger} from "./Log";

const log = new Logger("Filesystem File Store");

/**
 * Describes a found resource/file.
 */
export interface FilesystemFileStoreResult {
    /**
     * The contents of the file.
     */
    buffer: Buffer,

    /**
     * The liveness to use to set the cache control headers when returning this resource to the client.
     */
    liveness: Liveness,

    /**
     * Whether or not the resource should be accessible outside secure contexts (e.g: localhost).
     */
    secure: boolean,

    /**
     * The filename extension of the found resource.
     *
     * This is useful because:
     * (a) The extension is used to determine a mimetype to use for the Content-Type HTTP header.
     * (b) The filename might not be part of the requested URL (e.g: when the index file is returned).
     */
    filenameExtension: string
}

/**
 * A directory to serve from the filesystem.
 */
export class DirectoryFileStore {
    /**
     * Constructor :)
     *
     * @param path The path in the filesystem to serve from.
     * @param liveness The liveness to report in the result.
     * @param secure The secureness to report in the result.
     * @param index The file within the directory to use if a directory is requested.
     */
    constructor(private readonly path: string, private readonly liveness: Liveness, private readonly secure: boolean,
                private readonly index: string) {}

    /**
     * Try to find a given resource in the served directory.
     *
     * @param path The resource to look up *excluding* any prefix. I.e: a path within the directory given to the
     *        constructor.
     * @return A FilesystemFileStoreResult describing the found resource, or null if not found.
     */
    async get(resource: string): Promise<FilesystemFileStoreResult | null> {
        try {
            // If requesting a directory, actually load the index file.
            let path: string = `${this.path}/${resource}`;
            if ((await stat(path)).isDirectory()) {
                path = `${path}/${this.index}`;
            }

            // Load the file and return the descriptor with the file's contents.
            return {
                buffer: await readFile(path),
                liveness: this.liveness,
                secure: this.secure,
                filenameExtension: path.includes(".") ? path.slice(path.lastIndexOf(".") + 1) : ""
            };
        } catch {
            // One way or another, we couldn't load the resource. Probably it just couldn't be found though.
            return null;
        }
    }
}

/**
 * Look up resources to serve from the filesystem.
 */
export class FilesystemFileStore {
    /**
     * Add a directory to serve.
     *
     * @param prefix The prefix in the URL to match.
     * @param directoryFileStore The DirectoryFileStore to serve for the given prefix.
     */
    add(prefix: string, directoryFileStore: DirectoryFileStore): void {
        log.debug(`Adding "${prefix}"`);
        this.directoryFileStores.push([prefix, directoryFileStore]);
    }

    /**
     * Try to find the given resource in the filesystem, according to the previously added directories.
     *
     * The lookup and file reading is done (asynchronously) around the time of the call. This means if the filesystem
     * contents changes, the new contents can be served.
     *
     * @param resource The logical (i.e: in the HTTP resource space) resource path to look up.
     * @return A FilesystemFileStoreResult describing the found resource, or null if not found.
     */
    async get(resource: string): Promise<FilesystemFileStoreResult | null> {
        for (const [prefix, directoryFileStore] of this.directoryFileStores) {
            // See if this directory serves the given resource prefix.
            if (!resource.startsWith(prefix)) {
                continue;
            }

            // Try loading the resource from this directory.
            const result: FilesystemFileStoreResult | null =
                await directoryFileStore.get(resource.slice(prefix.length));
            if (result === null) {
                continue; // Maybe another directory will serve it.
            }
            return result;
        }

        // The requested resource was not found anywhere.
        return null;
    }

    // List of directories to search in for resources.
    private readonly directoryFileStores: [string, DirectoryFileStore][] = [];
}
