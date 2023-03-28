import {Player} from "./Main";
import {DebugHandler} from "./Debug";

type ControlsType = "native" | "js";

export interface PlayerOptions {
    secondarySource?: string;
    controls?: ControlsType;
    /** An optional string with which the IDs of all video player elements will be prefixed. Prefixing prevents ID
     * collisions if multiple players are created on one page. If omitted, a random integer will be used, so supply a
     * prefix if you want to select these elements by ID. */
    idPrefix?: string;
    debugHandler?: DebugHandler;
    onInitialisation?: (player: Player) => void;
    // TODO event listeners
}

/**
 * Construct an instance of the LLVS player
 * @param sourceURL the URL of the video stream
 * @param containerElement The DOM element into which the video player will be inserted. Pass either a DOM element that was created in JS, or else pass the ID of an html element.
 * @param options
 */
export default function createPlayer(
    sourceURL: string,
    containerElement: HTMLElement | string,
    options: PlayerOptions
): Player {
    // Defaults
    const controlsType = options?.controls ?? "js";

    // Try to find the container if necessary
    if (typeof containerElement === "string") {
        const maybeContainer = document.getElementById(containerElement);
        if (maybeContainer) {
            containerElement = maybeContainer;
        } else {
            console.error(`No DOM element exists with ID ${containerElement}. Appending video to document body!`);
            containerElement = insertNode(document.querySelector("body")!, "div", {className: "llvs-video"});
        }
    }

    const figure = insertNode(containerElement, "figure", {className: "video-figure"});
    const video = insertNode(figure, "video", {className: "video"});
    video.controls = controlsType === "native";

    if (options?.secondarySource) {
        const secondaryVideo = insertNode(figure, "video", {className: "secondary-video"});
        video.addEventListener("loadedmetadata", (): void => {
            secondaryVideo.muted = true;
            /* eslint "@typescript-eslint/no-unsafe-assignment": "off",
                      "@typescript-eslint/no-unsafe-member-access": "off",
                      "@typescript-eslint/no-unsafe-call": "off" */
            secondaryVideo.srcObject = (video as any).captureStream(); // Capture stream isn't in TypeScript's types.
            void secondaryVideo.play();
        });
    }

    // Create the player
    const player = new Player(sourceURL, video, process.env.NODE_ENV === "development");

    /* Performance/debug event handling. */
    if (process.env.NODE_ENV === "development" && options.debugHandler) {
        player.setDebugHandler(options.debugHandler);
    }

    // Error handling
    player.onError = (e: string): void => {
        console.error("An error happened: " + e);
    };

    player.init().then(() => {
        // Hook up the controls if necessary
        if (controlsType === "js") {
            // In case multiple videos are shown on the same page, we need IDs to be unique...
            const idPrefix = options?.idPrefix ?? (Math.random() * 100_000).toFixed(0);
            createControlPanel(figure, player, video, idPrefix);
        }

        options?.onInitialisation?.(player);
        player.start();
    }).catch((e) => {
        console.error(e instanceof Error ? "Failed to initialise player: " + e.message : "Failed to initialise player");
    });

    return player;
}

// Set up the control panel using JS (in place of the browsers' native controls)
function createControlPanel(figure: HTMLElement, player: Player, video: HTMLVideoElement, idPrefix: string): void {
    const controlsDiv = insertNode(figure, "div", {className: "video-controls"});

    // Create and wire up the angle and quality selectors
    const angle = insertSelector(controlsDiv, idPrefix + "-angle", "angle", player.getAngleOptions(), player.getAngle());
    angle.onchange = (): void => {
        player.setAngle(parseInt(angle.value));
    };
    const qualityOptionNames = getQualityOptionNames(player.getQualityOptions());
    const quality = insertSelector(controlsDiv, idPrefix + "-quality", "quality", qualityOptionNames, player.getQuality());
    quality.onchange = (): void => {
        player.setQuality(parseInt(quality.value));
    };

    // Mute button
    if (player.hasAudio()) {
        const mute = insertNode(controlsDiv, "button", {
            className: "mute-btn",
            innerText: player.getMuted() ? "Unmute" : "Mute"
        });
        mute.classList.add(player.getMuted() ? "unmute" : "mute");
        mute.onclick = (): void => {
            const nowMuted = !player.getMuted();
            player.setMuted(nowMuted);
            if (nowMuted) {
                mute.innerText = "Unmute";
                mute.classList.replace("mute", "unmute");
            } else {
                mute.innerText = "Mute";
                mute.classList.replace("unmute", "mute");
            }
        };
    }

    // Fullscreen button
    const fullscreen = insertNode(controlsDiv, "button", {
        className: "fullscreen",
        innerText: "Fullscreen"
    });
    fullscreen.onclick = (): void => {
        void video.requestFullscreen();
    };

    // Start/stop button
    const startStop = insertNode(controlsDiv, "button", {className: "start-stop", innerText: "Stop"});
    startStop.classList.add("stop");
    let isPlaying = true;
    startStop.onclick = (): void => {
        isPlaying = !isPlaying;
        if (isPlaying) {
            player.start();
            startStop.innerText = "Stop";
            startStop.classList.replace("start", "stop");
        } else {
            player.stop();
            startStop.innerText = "Start";
            startStop.classList.replace("stop", "start");
        }
    };
}

// Create a selector input with specified options and wire it up
function insertSelector(parent: HTMLElement, id: string, className: string, options: string[], index: number): HTMLSelectElement {
    const label = insertNode(parent, "label", {id: id + "-label", className});
    const selector = insertNode(label, "select", {id});
    options.forEach((name: string, nameIndex: number): void => {
        selector.innerHTML += `<option value="${nameIndex}">${name}</option>`;
    });
    selector.value = `${index}`;
    return selector;
}

interface NodeOptions {
    id?: string;
    className?: string;
    innerText?: string;
}

function insertNode(parent: HTMLElement, node: "video", options?: NodeOptions): HTMLVideoElement;
function insertNode(parent: HTMLElement, node: "label", options?: NodeOptions): HTMLLabelElement;
function insertNode(parent: HTMLElement, node: "select", options?: NodeOptions): HTMLSelectElement;
function insertNode(parent: HTMLElement, node: "button", options?: NodeOptions): HTMLButtonElement;
function insertNode(parent: HTMLElement, node: string, options?: NodeOptions): HTMLElement;
function insertNode(parent: HTMLElement, node: string, options?: NodeOptions): HTMLElement {
    const child = document.createElement(node);
    child.id = options?.id ?? "";
    child.className = options?.className ?? "";
    child.innerText = options?.innerText ?? "";
    parent.appendChild(child);
    return child;
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
