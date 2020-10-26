export class Deinterleaver {
    constructor(onData: (ArrayBuffer, number) => void) {
        this.onData = onData;
    }

    acceptData(newBuffer: Uint8Array): void {
        /* If we stopped part way through a chunk header last time, prepend it to the buffer. */
        let buffer: Uint8Array = newBuffer;
        if (this.remainderBuffer) {
            buffer = new Uint8Array(this.remainderBuffer.length + newBuffer.length);
            buffer.set(this.remainderBuffer);
            buffer.set(newBuffer, this.remainderBuffer.length);
            this.remainderBuffer = undefined;
        }

        /* Iterate, reading 1 chunk at a time. */
        for (let offset = 0; offset < buffer.length;) {
            // Handle the start of a new chunk. We might have started part way through a chunk, and might finish part
            // way through, so this condition isn't always true.
            if (this.currentOffset == this.currentLength) {
                const contentId: number = buffer[offset];
                const lengthWidth: number = ((): number => {
                    switch (Math.floor(contentId / 64)) {
                        case 0: return 1;
                        case 1: return 2;
                        case 2: return 4;
                        case 3: return 8;
                    }
                })();

                // Handle the case where the buffer stops part way through a chunk header.
                if (offset + lengthWidth + 1 > buffer.length) {
                    // Put the whole chunk header, up to as far as we have, into the remainder buffer.
                    this.remainderBuffer = buffer.slice(offset);
                    return;
                }

                // Otherwise, continue decoding the chunk header.
                this.currentLength = 0;
                let multiplier: number = 1;
                for (let i = 0; i < lengthWidth; i++) {
                    this.currentLength += buffer[offset + i + 1] * multiplier;
                    multiplier *= 256;
                }

                this.currentIndex = contentId % 64;
                this.currentOffset = 0;
                offset += lengthWidth + 1;
            }

            // Copy as much chunk data as we can. Note that we might not have just decoded a chunk. We might instead be
            // resuming one we've started on.
            const chunkLength = Math.min(buffer.length - offset, this.currentLength - this.currentOffset);

            // Store whatever of the chunk we currently have.
            this.onData(buffer.slice(offset, offset + chunkLength), this.currentIndex);
            this.currentOffset += chunkLength;
            offset += chunkLength;
        }
    }

    private readonly onData: (ArrayBuffer, number) => void;

    private currentIndex = 0; // Index of the current chunk's content.
    private currentOffset = 0; // Offset within the current chunk.
    private currentLength = 0; // Length of the current chunk.
    private remainderBuffer: Uint8Array;
}
