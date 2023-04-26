import * as Debug from "../Debug";
import {API} from "live-video-streamer-common";

/**
 * Deinterleave timestamp information.
 *
 * This is useful for estimating network jitter. It can also be used to estimate the network delay, but only if the
 * local clock and remote clock are both accurate.
 */
export interface TimestampInfo {
    /**
     * Timestamp encoded in the interleave in ms. This is calculated from the local clock and the number of bits of
     * timestamp that actually got sent.
     */
    sentTimestamp: number,

    /**
     * Timestamp when we received the end of the chunk in ms.
     */
    endReceivedTimestamp: number,

    /**
     * Whether or not this is the first chunk of data in an interleave.
     */
    firstForInterleave: boolean
}

export class Deinterleaver {
    constructor(onData: (data: ArrayBuffer, index: number) => void,
                onControlChunk: (data: ArrayBuffer, controlChunkType: number) => void,
                onTimestamp: (timestampInfo: TimestampInfo) => void, description: string) {
        this.onData = onData;
        this.onControlChunk = onControlChunk;
        this.onTimestamp = onTimestamp;
        this.description = description;

        if (process.env["NODE_ENV"] === "development") {
            this.checksums = new Map<number, Debug.Addler32>();
        }
    }

    acceptData(newBuffer: Uint8Array): void {
        /* If we stopped part way through a chunk header last time, prepend it to the buffer. */
        let buffer: Uint8Array = newBuffer;
        if (this.remainderBuffer) {
            buffer = new Uint8Array(this.remainderBuffer.length + newBuffer.length);
            buffer.set(this.remainderBuffer);
            buffer.set(newBuffer, this.remainderBuffer.length);
            this.remainderBuffer = null;
        }

        /* Iterate, reading 1 chunk at a time. */
        for (let offset = 0; offset < buffer.length;) {
            // Handle the start of a new chunk. We might have started part way through a chunk, and might finish part
            // way through, so this condition isn't always true.
            if (this.currentOffset === this.currentLength) {
                const contentId: number = buffer[offset]!;
                const lengthWidth: number = ((): number => {
                    switch (Math.floor(contentId / 64)) {
                        case 0: return 1;
                        case 1: return 2;
                        case 2: return 4;
                        case 3: return 8;
                    }
                    return 0; // Unreachable.
                })();
                const sentTimestampWidth: number = ((Math.floor(contentId / 32) % 2) === 1) ? 8 : 0;

                // Handle the case where the buffer stops part way through a chunk header.
                if (offset + lengthWidth + sentTimestampWidth + 1 > buffer.length) {
                    // Put the whole chunk header, up to as far as we have, into the remainder buffer.
                    this.remainderBuffer = buffer.slice(offset);
                    return;
                }

                // Otherwise, continue decoding the chunk header.
                let currentLength: number = 0;
                let multiplier: number = 1;
                for (let i = 0; i < lengthWidth; i++) {
                    currentLength += buffer[offset + i + 1]! * multiplier;
                    multiplier *= 256;
                }

                let sentTimestamp: number | null = null;
                if (sentTimestampWidth > 0) {
                    sentTimestamp = 0;
                    multiplier = 1;
                    for (let i = 0; i < sentTimestampWidth; i++) {
                        sentTimestamp += buffer[offset + lengthWidth + i + 1]! * multiplier;
                        multiplier *= 256;
                    }
                }

                const currentIndex = contentId % 32;
                const headerLength = lengthWidth + sentTimestampWidth + 1;

                // Only handle complete control steam chunks.
                if (currentIndex === 31 && buffer.length - offset - headerLength < currentLength) {
                    // Put the whole chunk header, up to as far as we have, into the remainder buffer, so we can process
                    // the entire control chunk at once.
                    this.remainderBuffer = buffer.slice(offset);
                    return;
                }

                // We're going to process this chunk.
                this.currentIndex = currentIndex;
                this.currentOffset = 0;
                this.currentLength = currentLength;
                if (sentTimestamp !== null) {
                    this.sentTimestamp = sentTimestamp;
                }
                offset += headerLength;
            }

            // Copy as much chunk data as we can. Note that we might not have just decoded a chunk. We might instead be
            // resuming one we've started on.
            const chunkLength = Math.min(buffer.length - offset, this.currentLength - this.currentOffset);
            if (chunkLength === 0 && this.currentLength !== 0) {
                continue; // Don't emit a spurious empty chunk.
            }

            // Handle the chunk data we've received.
            let data: Uint8Array | null = null; // Also used to print debug checksums.
            if (this.currentIndex === 31) {
                const controlChunkType: number = buffer[offset]!; // the first byte of the chunk is its type.
                if (controlChunkType !== API.ControlChunkType.discard) {
                    this.onControlChunk(buffer.slice(offset + 1, offset + chunkLength), controlChunkType);
                }
            } else {
                // Store whatever of the chunk we currently have.
                data = buffer.slice(offset, offset + chunkLength);
                this.onData(data, this.currentIndex);
            }
            this.currentOffset += chunkLength;
            offset += chunkLength;

            // If we've just pushed the last of the data for this chunk.
            if (this.sentTimestamp !== null) {
                this.onTimestamp({
                    sentTimestamp: this.sentTimestamp / 1000,
                    endReceivedTimestamp: Date.now(),
                    firstForInterleave: this.firstTimestamp
                });
                this.sentTimestamp = null;
                this.firstTimestamp = false;
            }

            // Print checksums.
            if (process.env["NODE_ENV"] === "development") {
                if (data === null) {
                    continue;
                }
                if (!this.checksums!.has(this.currentIndex)) {
                    this.checksums!.set(this.currentIndex, new Debug.Addler32());
                }
                if (chunkLength === 0) {
                    const checksum = this.checksums!.get(this.currentIndex)!;
                    console.log(`[Deinterleave Checksum] ${this.description} ` +
                                `checksum: ${checksum.getChecksum()}, length: ${checksum.getLength()}`);
                } else {
                    this.checksums!.get(this.currentIndex)!.update(data);
                }
            }
        }
    }

    private readonly onData: (data: ArrayBuffer, index: number) => void;
    private readonly onControlChunk: (data: ArrayBuffer, controlChunkType: number) => void;
    private readonly onTimestamp: (timestampInfo: TimestampInfo) => void;
    private readonly description: string;

    private currentIndex = 0; // Index of the current chunk's content.
    private currentOffset = 0; // Offset within the current chunk.
    private currentLength = 0; // Length of the current chunk.
    private remainderBuffer: Uint8Array | null = null;
    private sentTimestamp: number | null = null; // Sent timestamp in µs.
    private firstTimestamp: boolean = true;

    private readonly checksums: Map<number, Debug.Addler32> | undefined;
}
