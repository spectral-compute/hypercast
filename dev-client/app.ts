import {Player} from "live-video-streamer-client";
import {AppDebugHandler} from "./debug";

/* Set an error handler for the video element. */
const video = document.getElementById("video")! as HTMLDivElement;

/* Error handling. */
function onError(e: string): void {
    document.getElementsByTagName("body")[0]!.innerHTML = "An error happened: " + e;
}

/* Update the UI whenever the player's settings change. */
function onUpdate(elective: boolean): void {
    setupUi(elective);
    document.getElementById("stop")!.hidden = false;
}

/* Called when a user object is received. */
function onBroadcastObject(o: any) {
    console.log("Received user broadcast object.");
    console.log(o);
    document.getElementById("user_object")!.innerText =
        `User object: ${(typeof o === "object") ? JSON.stringify(o) : o}`;
}

// Get the custom server origin from query parameters.
const urlParams = new URLSearchParams(window.location.search);
const server = urlParams.get("server") ?? undefined;

/* Create the player. */
const player = new Player(video, {server});
player.on("error", (e) => onError(e.e.message));
player.on("broadcast_object", (e) => onBroadcastObject(e.o));
player.on("update", (e) => onUpdate(e.elective));

(document.getElementById("server_origin")! as HTMLSpanElement).innerText = player.getServer();

/* Performance/debug event handling. */
if (process.env["NODE_ENV"] === "development") {
    player.setDebugHandler(new AppDebugHandler());
}

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

function setupNamedSelector(
    id: string,
    options: {[optionValue: string]: string | null},
    selectedOption: string,
    visible: boolean = true
): void {
    const selector = document.getElementById(id)! as HTMLSelectElement;
    selector.innerHTML = "";
    Object.entries(options).forEach(([optionValue, optionName]): void => {
        const isSelected = optionValue === selectedOption;
        selector.innerHTML += `<option value="${optionValue}" ${isSelected ? "selected" : ""}>${optionName ?? optionValue}</option>`;
    });
    selector.value = selectedOption;
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

    const channelIndex = player.getChannelIndex();
    if (channelIndex) {
        setupNamedSelector("channel", channelIndex, player.getChannel());
    }

    setupSelector("quality", getQualityOptionNames(player.getQualityOptions()), player.getQuality());

    /* Update the mute button. */
    document.getElementById("mute")!.hidden = !player.hasAudio() || player.getMuted();
    document.getElementById("unmute")!.hidden = !player.hasAudio() || !player.getMuted();

    /* Update the change info. */
    const quality = player.getQualityOptions()[player.getQuality()]!;
    document.getElementById("change")!.innerHTML =
        `${elective ? "Elective " : "Non-elective"} change to ` +
        `quality ${quality[0]!}x${quality[1]!}`;

    /* Show the UI. */
    document.getElementById("innerui")!.hidden = false;
}

/* Wire up the UI's outputs. */
const channel = document.getElementById("channel")! as HTMLSelectElement;
channel.onchange = async () => {
    await player.setChannel(channel.value);
};
const quality = document.getElementById("quality")! as HTMLSelectElement;
quality.onchange = (): void => {
    player.setQuality(parseInt(quality.value));
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

const fullscreen = document.getElementById("fullscreen")!;
fullscreen.onclick = (): void => {
    void video.requestFullscreen();
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
