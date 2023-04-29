import {sleep, waitForEvent} from "live-video-streamer-common";

/**
 * Wait for a media element to reach the requested PTS.
 *
 * This method can return *slightly* early.
 *
 * @param pts The presentation timestamp to wait for.
 * @param mediaElement The media element to wait for.
 * @param signal The signal that is used to abort waiting.
 */
export async function waitForPts(pts: number, mediaElement: HTMLMediaElement, signal: AbortSignal): Promise<void> {
    while (true) {
        /* Figure out how long to wait for. */
        const seconds = pts - mediaElement.currentTime;

        /* If we're very close to the time, or after it, then don't wait. The very close part of this is so we don't get
           stuck in many iterations of "very nearly". */
        if (seconds <= 0.002) {
            break;
        }

        /* If the video is paused, wait for it to be unpaused. */
        if (mediaElement.paused) {
            await waitForEvent("playing", mediaElement, signal);
            continue;
        }

        /* Wait until live playback would get us to the correct PTS. */
        await sleep(seconds * 1000, signal);
    }
}
