/* A class that wraps a ReadableStream to provide an event interface, data holding, and timing
   information. */
class ReadableStreamWrapper {
    // Constructs in the paused state. No events will happen until unpaused. The onData callback is called
    // when there's new data, and receives an ArrayBuffer. The onDone callback is called when the stream
    // ends, and receives an estimate of its download rate in bytes/s.
    constructor(stream, onData: (data: ArrayBuffer) => void, onDone: (rate: number) => void) {
        this.onData = onData;
        this.onDone = onDone;
        this.read(stream);
    }

    unpause(): void {
        if (!this.paused) {
            return;
        }

        this.paused = false;
        for (let i = 0; i < this.buffers.length; i++) {
            this.onData(this.buffers[i]);
        }
        this.buffers = new Array<ArrayBuffer>();
        if (this.done) {
            this.finish();
        }
    }

    private read(stream: ReadableStreamDefaultReader): void {
        stream.read().then(({done, value}: {done: boolean, value: ArrayBuffer}): void => {
            // Handle the case where this is the notification that there's no more data.
            if (done) {
                this.done = true;
                if (!this.paused) {
                    // If we're not paused, then we need to call the handler ourselves.
                    this.finish();
                }
                return;
            }

            // Update timing information. Don't include the first chunk, because this stream might have been
            // created significantly before the first byte was available.
            if (this.timingStart == 0) {
                this.timingStart = Date.now();
                this.timingEnd = this.timingStart;
            }
            else {
                this.timingEnd = Date.now();
                this.timingByteCount += value.byteLength;
            }

            // If we're paused, then just stash the data for later. Otherwise, call the event handler now.
            if (this.paused) {
                this.buffers.push(value);
            }
            else {
                this.onData(value);
            }

            // Read more.
            this.read(stream);
        });
    }

    private finish(): void {
        this.onDone(this.timingByteCount / (this.timingEnd - this.timingStart));
    }

    private onData: (data: ArrayBuffer) => void;
    private onDone: (rate: number) => void;

    private buffers: Array<ArrayBuffer> = new Array<ArrayBuffer>();
    private paused: boolean = true;
    private done: boolean = false;
    private timingStart: number = 0;
    private timingEnd: number = 0;
    private timingByteCount: number = 0;
};

/* A class that accepts input from multiple ReadableStreams and outputs them in sequence. */
export class MultiStreamReader {
    constructor(callback) {
        this.callback = callback;
    }

    setOnDone(callback: () => void): void {
        this.onDone = callback;
    }

    addStream(stream: ReadableStream, onDone: (rate: number) => void = () => {}): void {
        const streamWrapper = new ReadableStreamWrapper(stream.getReader(), (data: ArrayBuffer) => {
            this.callback(data);
        }, (rate: number) => {
            // Call the callback for this stream being done.
            onDone(rate);

            // Remove the head stream. That *must* be the stream that generated this callback because only
            // the first stream is ever unpaused, and only unpaused streams are allowed to call their
            // callbacks.
            this.streams = this.streams.slice(1);

            // Figure out what to do next.
            if (this.streams.length > 0) {
                this.streams[0].unpause();
            }
            else if (this.onDone !== null) {
                this.onDone();
            }
        });
        this.streams.push(streamWrapper);
        if (this.streams.length == 1) {
            streamWrapper.unpause();
        }
    }

    private callback: (data: ArrayBuffer) => void;
    private streams: Array<ReadableStreamWrapper> = new Array<ReadableStreamWrapper>();
    private onDone: () => void = null;
};
