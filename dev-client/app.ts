import * as lvsc from "live-video-streamer-client";
import {AppDebugHandler} from "./debug";

/* Configuration :) */
const infoUrl = process.env.INFO_URL;
(document.getElementById("info_url")! as HTMLSpanElement).innerText = infoUrl;

/* Set an error handler for the video element. */
const video = document.getElementById("video")! as HTMLVideoElement;
const audio = document.getElementById("audio")! as HTMLAudioElement;
video.addEventListener("error", (): void => {
    document.getElementById("change")!.innerText = "Video error: " + video.error!.message;
});
audio.addEventListener("error", (): void => {
    document.getElementById("change")!.innerText = "Audio error: " + audio.error!.message;
});

/* Attach secondary view. */
const secondaryVideo = document.getElementById("secondaryVideo") as HTMLVideoElement;
if (process.env.SECONDARY_VIDEO) {
    video.addEventListener("loadedmetadata", (): void => {
        secondaryVideo.muted = true;
        /* eslint "@typescript-eslint/no-unsafe-assignment": "off", "@typescript-eslint/no-unsafe-member-access": "off",
                  "@typescript-eslint/no-unsafe-call": "off" */
        secondaryVideo.srcObject = (video as any).captureStream(); // Capture stream isn't in TypeScript's types.
        void secondaryVideo.play();
    });
} else {
    secondaryVideo.hidden = true;
}

/* Create the player. */
const player = new lvsc.Player(infoUrl, video, process.env.SECONDARY_AUDIO ? audio : null,
                               process.env.NODE_ENV === "development");

/* Performance/debug event handling. */
if (process.env.NODE_ENV === "development") {
    player.setDebugHandler(new AppDebugHandler());
}

/* Error handling. */
player.onError = (e: string): void => {
    document.getElementsByTagName("body")[0]!.innerHTML = "An error happened: " + e;
};

/* A function to wire up a selector. */
function setupSelector(id: string, options: string[], index: number, visible: boolean = true): void {
    const selector = document.getElementById(id)! as HTMLSelectElement;
    selector.innerHTML = "";
    options.forEach((name: string, nameIndex: number): void => {
        selector.innerHTML += `<option value="${nameIndex}">${name}</option>`;
    });
    selector.value = `${index}`;
    selector.hidden = !visible;
    document.getElementById(id + "Label")!.hidden = !visible;
}

/**
 * Convert an array of resolutions to an array of strings.
 */
function getQualityOptionNames(qualityOptions: [number, number][]): string[] {
    const result: string[] = [];
    qualityOptions.forEach((res: [number, number]) => {
        result.push(`${res[0]}x${res[1]}`);
    });
    return result;
}

/* A function to set up the UI. */
function setupUi(elective: boolean): void {
    /* Update the selectors. */
    setupSelector("angle", player.getAngleOptions(), player.getAngle());
    setupSelector("quality", getQualityOptionNames(player.getQualityOptions()), player.getQuality());
    setupSelector("latency", player.getLatencyOptions(), player.getLatency(), player.getLatencyAvailable());

    /* Update the mute button. */
    document.getElementById("mute")!.hidden = !player.hasAudio() || player.getMuted();
    document.getElementById("unmute")!.hidden = !player.hasAudio() || !player.getMuted();

    /* Update the change info. */
    const quality = player.getQualityOptions()[player.getQuality()]!;
    document.getElementById("change")!.innerHTML =
        `${elective ? "Elective " : "Non-elective"} change to ` +
        `angle ${player.getAngleOptions()[player.getAngle()]!}, ` +
        `quality ${quality[0]!}x${quality[1]!}`;

    /* Show the UI. */
    document.getElementById("innerui")!.hidden = false;
}

/* Update the UI whenever the player's settings change. */
player.onStartPlaying = (elective: boolean): void => {
    setupUi(elective);
    document.getElementById("stop")!.hidden = false;
};

/* Wire up the UI's outputs. */
const angle = document.getElementById("angle")! as HTMLSelectElement;
angle.onchange = (): void => {
    player.setAngle(parseInt(angle.value));
};
const quality = document.getElementById("quality")! as HTMLSelectElement;
quality.onchange = (): void => {
    player.setQuality(parseInt(quality.value));
};
const latency = document.getElementById("latency")! as HTMLSelectElement;
latency.onchange = (): void => {
    player.setLatency(parseInt(latency.value));
};

const mute = document.getElementById("mute")!;
const unmute = document.getElementById("unmute")!;
mute.onclick = (): void => {
    player.setMuted(true);
    mute.hidden = true;
    unmute.hidden = false;
};
unmute.onclick = (): void => {
    player.setMuted(false);
    unmute.hidden = true;
    mute.hidden = false;
};

const stop = document.getElementById("stop")!;
const start = document.getElementById("start")!;
stop.onclick = (): void => {
    document.getElementById("innerui")!.hidden = true;
    stop.hidden = true;
    player.stop();
    start.hidden = false;
};

start.onclick = (): void => {
    start.hidden = true;
    player.start();
};

/* Start. */
async function init(): Promise<void> {
    await player.init();
    setupUi(false);
    player.start();
}
void init();
