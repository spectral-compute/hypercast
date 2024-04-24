interface ManagedMediaSource {};

declare var ManagedMediaSource: {
    prototype: ManagedMediaSource;
    new(): ManagedMediaSource;
};

export function getMediaSource(): MediaSource {
    if (typeof MediaSource !== "undefined") {
        return new MediaSource();
    }
    if (typeof ManagedMediaSource !== "undefined") {
        return new ManagedMediaSource() as MediaSource;
    }
    throw Error("No media source extensions or managed media source API available.");
}
