import * as lvsc from 'live-video-streamer-client';

/* Configuration :) */
const infoUrl = 'http://localhost:8080/live/info.json';

/* Set an error handler for the video element. */
const video = document.getElementById('video')! as HTMLVideoElement;
const audio = document.getElementById('audio')! as HTMLAudioElement;
video.addEventListener('error', () => {
    document.getElementById('change')!.innerText = 'Video error: ' + video.error!.message;
});
audio.addEventListener('error', () => {
    document.getElementById('change')!.innerText = 'Audio error: ' + audio.error!.message;
});

/* Attach secondary view. */
video.addEventListener('loadedmetadata', () => {
    const secondaryVideo = document.getElementById('secondaryVideo') as HTMLVideoElement;
    secondaryVideo.muted = true;
    secondaryVideo.srcObject = (video as any).captureStream(); // Capture stream isn't in TypeScript's types.
    secondaryVideo.play();
});

/* Create the player. */
const player = new lvsc.Player(infoUrl, video, audio, () => {
    setupUi(false);
    player.start();
}, true); // Omit final argument for deployment.

/* Error handling. */
player.onError = (e: any) => {
    document.getElementsByTagName('body')[0]!.innerHTML = 'An error happened: ' + e;
};

/* A function to wire up a selector. */
function setupSelector(id: string, options: Array<string>, index: number, visible: boolean = true) {
    const selector = document.getElementById(id)! as HTMLSelectElement;
    selector.innerHTML = '';
    options.forEach((name: string, index: number) => {
        selector.innerHTML += '<option value="' + index + '">' + name + '</option>';
    });
    selector.value = '' + index;
    selector.hidden = !visible;
    document.getElementById(id + 'Label')!.hidden = !visible;
}

/**
 * Convert an array of resolutions to an array of strings.
 */
function getQualityOptionNames(qualityOptions: Array<[number, number]>): Array<string> {
    let result = new Array<string>();
    qualityOptions.forEach((res: [number, number]) => {
        result.push(`${res[0]}x${res[1]}`);
    });
    return result;
}

/* A function to set up the UI. */
function setupUi(elective: boolean) {
    /* Update the selectors. */
    setupSelector('angle', player.getAngleOptions(), player.getAngle());
    setupSelector('quality', getQualityOptionNames(player.getQualityOptions()), player.getQuality());
    setupSelector('latency', player.getLatencyOptions(), player.getLatency(), player.getLatencyAvailable());

    /* Update the mute button. */
    document.getElementById('mute')!.hidden = !player.hasAudio() || player.getMuted();
    document.getElementById('unmute')!.hidden = !player.hasAudio() || !player.getMuted();

    /* Update the change info. */
    document.getElementById('change')!.innerHTML =
        `${elective ? "Elective " : "Non-elective"} change to ` +
        `angle ${player.getAngleOptions()[player.getAngle()]}, ` +
        `quality ${player.getQualityOptions()[player.getQuality()]}`;

    /* Show the UI. */
    document.getElementById('innerui')!.hidden = false;
}

/* Update the UI whenever the player's settings change. */
player.onStartPlaying = (elective: boolean) => {
    setupUi(elective);
    document.getElementById('stop')!.hidden = false;
};

/* Wire up the UI's outputs. */
const angle = document.getElementById('angle')! as HTMLSelectElement;
angle.onchange = () => {
    player.setAngle(parseInt(angle.value));
};
const quality = document.getElementById('quality')! as HTMLSelectElement;
quality.onchange = () => {
    player.setQuality(parseInt(quality.value));
};
const latency = document.getElementById('latency')! as HTMLSelectElement;
latency.onchange = () => {
    player.setLatency(parseInt(latency.value));
};

const mute = document.getElementById('mute')!;
const unmute = document.getElementById('unmute')!;
mute.onclick = () => {
    player.setMuted(true);
    mute.hidden = true;
    unmute.hidden = false;
};
unmute.onclick = () => {
    player.setMuted(false);
    unmute.hidden = true;
    mute.hidden = false;
};

const stop = document.getElementById('stop')!;
const start = document.getElementById('start')!;
stop.onclick = () => {
    document.getElementById('innerui')!.hidden = true;
    stop.hidden = true;
    player.stop();
    start.hidden = false;
};

start.onclick = () => {
    start.hidden = true;
    player.start();
};
