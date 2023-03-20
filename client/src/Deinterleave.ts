import * as Debug from "./Debug";

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
                onTimestamp: (timestampInfo: TimestampInfo) => void, description: string) {
        this.onData = onData;
        this.onTimestamp = onTimestamp;
        this.description = description;

        if (process.env.NODE_ENV === "development") {
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
                this.currentLength = 0;
                let multiplier: number = 1;
                for (let i = 0; i < lengthWidth; i++) {
                    this.currentLength += buffer[offset + i + 1]! * multiplier;
                    multiplier *= 256;
                }

                if (sentTimestampWidth > 0) {
                    this.sentTimestamp = 0;
                    multiplier = 1;
                    for (let i = 0; i < sentTimestampWidth; i++) {
                        this.sentTimestamp += buffer[offset + lengthWidth + i + 1]! * multiplier;
                        multiplier *= 256;
                    }
                }

                this.currentIndex = contentId % 32;
                this.currentOffset = 0;
                offset += lengthWidth + sentTimestampWidth + 1;
            }

            // Copy as much chunk data as we can. Note that we might not have just decoded a chunk. We might instead be
            // resuming one we've started on.
            const chunkLength = Math.min(buffer.length - offset, this.currentLength - this.currentOffset);
            if (chunkLength === 0 && this.currentLength !== 0) {
                continue; // Don't emit a spurious empty chunk.
            }

            // Store whatever of the chunk we currently have.
            const data = buffer.slice(offset, offset + chunkLength);
            if (this.currentIndex !== 31) {
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
            if (process.env.NODE_ENV === "development") {
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
