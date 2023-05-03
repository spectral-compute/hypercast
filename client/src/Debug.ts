import {BufferControl} from "./live/BufferCtrl";
import {TimestampInfo} from "./live/Deinterleave";
import {ReceivedInfo} from "./live/SegmentDownloader";

/**
 * Adler32 checksum for debugging.
 *
 * https://datatracker.ietf.org/doc/html/rfc1950
 */
export class Adler32 {
    update(data: ArrayBuffer): void {
        const data8 = new Uint8Array(data);
        const prime = 65521;
        for (const d of data8) {
            this.s1 = (this.s1 + d) % prime;
            this.s2 = (this.s1 + this.s2) % prime;
        }
        this.length += data8.length;
    }

    getChecksum(): string {
        return (this.s2 * 65536 + this.s1).toString(16).padStart(8, "0");
    }

    getLength(): number {
        return this.length;
    }

    private s1: number = 1;
    private s2: number = 0;
    private length: number = 0;
}

/**
 * Information about the buffer control status at the start of a buffer control
 * tick.
 *
 * This can capture useful information about the tick, but it is especially
 * meant to capture information that would be difficult to get via other buffer
 * control interfaces, such as the initial buffer length (which might change
 * during the time processing takes).
 */
export interface BufferControlTickInfo {
    /**
     * When the tick happened.
     */
    timestamp: number;

    /**
     * The length of the primary media element's buffer at the start of the
     * tick.
     */
    primaryBufferLength: number;

    /**
     * Whether a catch-up event happened.
     */
    catchUp: boolean;

    /**
     * Whether the initial seek event happened.
     */
    initialSeek: boolean;
}

/**
 * Interface for debugging the player.
 */
export interface DebugHandler {
    /**
     * Called once the player has created the buffer control object.
     *
     * @param bctrl The created buffer control object. This is useful for
     *              getting information about the current state of buffer
     *              control.
     */
    setBufferControl(bctrl: BufferControl): void;

    /**
     * Called after a buffer control tick happens.
     */
    onBufferControlTick(tickInfo: BufferControlTickInfo): void;

    /**
     * Called when a new timestamp information object is available.
     *
     * @param timestampInfo The new timestamp information object.
     */
    onTimestamp(timestampInfo: TimestampInfo): void;

    /**
     * Called when new data is recived from the stream.
     *
     * @param receivedInfo Describes the received data.
     */
    onReceived(receivedInfo: ReceivedInfo): void;
}
