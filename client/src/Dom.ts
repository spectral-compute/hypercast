import {Player, PlayerSourceOptions} from "./Player";
import {DebugHandler} from "./Debug";

export interface CreatePlayerOptions {
    secondarySource?: string;
    debugHandler?: DebugHandler;
    onInitialisation?: (player: Player) => void;
    // TODO event listeners
}

/**
 * Construct an instance of the RISE player with a control panel
 * @param channelsUrl the URL to the index of channels
 * @param containerElement The DOM element into which the video player will be inserted. Pass either a DOM element that was created in JS, or else pass the ID of an html element.
 * @param options
 */
export async function createPlayer(
    containerElement: HTMLDivElement | string,
    source?: PlayerSourceOptions,
    options?: CreatePlayerOptions,
): Promise<Player> {
    // Try to find the container if necessary
    if (typeof containerElement === "string") {
        const maybeContainer = document.querySelector(`div#${containerElement}`) as HTMLDivElement | null;
        if (maybeContainer) {
            containerElement = maybeContainer;
        } else {
            console.error(`No DOM element exists with ID ${containerElement}. Appending video to document body!`);
            containerElement = insertNode(document.querySelector("body")!, "div", {className: "llvs-video"});
        }
    }

    // Error handling
    function onError(e: string): void {
        console.error("An error happened: " + e);
    }

    // Create the player
    const player = new Player(containerElement, source, {onError});
    const controlsDiv = insertNode(containerElement, "div", {className: "video-controls"});

    /* Performance/debug event handling. */
    if (options?.debugHandler) {
        player.setDebugHandler(options.debugHandler);
    }

    try {
        await player.init();
    } catch (e) {
        // TODO cope with failure to initialise
        console.error(e instanceof Error ? "Failed to initialise player: " + e.message : "Failed to initialise player");
    }

    const onUpdate = () => {
        // Erase the original controls
        controlsDiv.innerHTML = "";
        // Recreate the controls
        createControlPanel(controlsDiv, player);
    };

    player.onUpdate = onUpdate;

    options?.onInitialisation?.(player);
    player.start();

    return player;
}

// Set up the control panel using JS (in place of the browsers' native controls)
function createControlPanel(controlsDiv: HTMLDivElement, player: Player): void {
    // Create and wire up the channel selector
    const channelIndex = player.getChannelIndex();
    if (channelIndex) {
        const channel = insertNamedSelector(controlsDiv, "channel", channelIndex, player.getChannel());
        channel.onchange = async () => {
            await player.setChannel(channel.value);
        };
    }

    // Create and wire up the quality selector
    const qualityOptionNames = getQualityOptionNames(player.getQualityOptions());
    const quality = insertSelector(controlsDiv, "quality", qualityOptionNames, player.getQuality());
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
        void player.requestFullscreen();
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

// Create a selector input with specified options (index-based) and wire it up
function insertSelector(parent: HTMLElement, className: string, options: string[], index: number): HTMLSelectElement {
    const selector = insertNode(parent, "select", {className});
    options.forEach((name: string, nameIndex: number): void => {
        selector.innerHTML += `<option value="${nameIndex}">${name}</option>`;
    });
    selector.value = `${index}`;
    return selector;
}

// Create a selector input with specified options (name-based) and wire it up
function insertNamedSelector(
    parent: HTMLElement,
    className: string,
    options: {[optionValue: string]: string | null /* option name */},
    selectedValue: string
): HTMLSelectElement {
    const selector = insertNode(parent, "select", {className});
    Object.entries(options).forEach(([optionValue, optionName]): void => {
        const isSelected = optionValue === selectedValue;
        selector.innerHTML += `<option value="${optionValue}" ${isSelected ? "selected" : ""}>${optionName ?? optionValue}</option>`;
    });
    selector.value = selectedValue;
    return selector;
}

interface NodeOptions {
    id?: string;
    className?: string;
    innerText?: string;
}

function insertNode(parent: HTMLElement, node: "div", options?: NodeOptions): HTMLDivElement;
function insertNode(parent: HTMLElement, node: "label", options?: NodeOptions): HTMLLabelElement;
function insertNode(parent: HTMLElement, node: "select", options?: NodeOptions): HTMLSelectElement;
function insertNode(parent: HTMLElement, node: "button", options?: NodeOptions): HTMLButtonElement;
function insertNode(parent: HTMLElement, node: string, options?: NodeOptions): HTMLElement;
function insertNode(parent: HTMLElement, node: string, options?: NodeOptions): HTMLElement {
    const child = document.createElement(node);
    if (options?.id) {
        // Assigning an empty string would add an empty id, so let's ony assign if we have a value
        child.id = options.id;
    }
    if (options?.className) {
        // Assigning an empty string would add an empty class, so let's ony assign if we have a value
        child.className = options.className;
    }
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
