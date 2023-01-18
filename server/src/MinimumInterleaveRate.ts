import assert = require("assert");

interface History {
    time: number;
    size: number;
}

export class MinimumInterleaveRate {
    /**
     * Start tracking interleave rate, and pushing padding data if the rate drops too low.
     *
     * @param onPad The function to call when new data is to be added.
     * @param minimumRate The minimum rate, in bytes per second, to maintain.
     * @param windowSize The window over which the rate is measured, in ms.
     */
    constructor(private readonly onPad: (b: Buffer) => void, private readonly minimumRate: number,
                private readonly windowSize: number) {}

    /**
     * Start tracking.
     */
    start(): void {
        assert(this.interval === null);
        assert(this.history.length === 0);
        this.interval = setInterval((): void => {
            this.onPoll();
        }, this.windowSize / 2);
    }

    /**
     * Stop tracking.
     */
    stop(): void {
        assert(this.interval !== null);
        clearTimeout(this.interval);
        this.interval = null;
        this.history = [];
    }

    /**
     * Determine whether tracking is currently running.
     */
    isRunning(): boolean {
        return this.interval !== null;
    }

    /**
     * Tell the rate tracker that new data has been added to the interleave.
     *
     * This is expected to be called when padding is added too.
     *
     * @param size The size of the data added.
     */
    addData(size: number): void {
        assert(this.interval !== null);
        this.history.push({time: Date.now(), size: size});
    }

    /**
     * Checks the interleave rate history, and adds padding data if necessary.
     */
    private onPoll(): void {
        /* Clear any history older than the window size. */
        const now = Date.now();
        let slice: number = 0;
        while (slice < this.history.length && this.history[slice]!.time < now - this.windowSize) {
            slice++;
        }
        this.history = this.history.slice(slice);

        /* See how much is in the history now. */
        let totalSize: number = 0;
        this.history.forEach((element: History): void => {
            totalSize += element.size;
        });

        /* If the rate is high enough, then we're done. */
        const minimumSize = this.minimumRate * this.windowSize / 1000;
        if (minimumSize <= totalSize) {
            return;
        }

        /* Otherwise, we need to add some padding data. */
        const randBytes = 4; // Idk how good the LSBs are, so let's just do 4 bytes at a time. Theoretical maximum is 6.
        const buffer = Buffer.alloc(Math.ceil((minimumSize - totalSize) / randBytes) * randBytes);
        for (let i = 0; i < buffer.length; i += randBytes) {
            // The data is chosen to be random so that it doesn't get compressed.
            buffer.writeUIntLE(Math.floor(Math.random() * (2 ** (8 * randBytes))), i, randBytes);
        }
        this.onPad(buffer);
    }

    /**
     * The timeout object for the polling.
     */
    private interval: NodeJS.Timeout | null = null;

    /**
     * A history of recent data additions.
     *
     * This is a pair [time, size]
     */
    private history: History[] = [];
}
