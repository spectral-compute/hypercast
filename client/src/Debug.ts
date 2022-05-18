/**
 * Addler32 checksum for debugging.
 *
 * https://datatracker.ietf.org/doc/html/rfc1950
 */
export class Addler32 {
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
