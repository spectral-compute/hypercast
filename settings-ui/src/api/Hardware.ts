import { DecklinkPort, DECKLINK_PORT_SETTINGS } from "../Constants";

export enum PortConnector {
    SDI = "SDI"
}

export interface MediaSourceInfo {
    framerateNumerator: number;
    framerateDenominator: number;
    width: number;
    height: number;
}

export interface PortDescriptor {
    name: string;

    connector: PortConnector;

    // Description of what's connected, if anything.
    connectedMediaInfo?: MediaSourceInfo;
}

// Describes the machine the server is running on, and its current status.
// Note that this can in general change (although the server may not yet support that). People
// can do stuff like plug in a decklink and suddenly they've got more SDI ports. Not a relevant
// concern on our custom hardware, however...
export interface MachineInfo {
    inputPorts: {[K in DecklinkPort]: PortDescriptor}

    // Whether or not streaming is actually turned on.
    isStreaming: boolean;
}

export function inputUrlToSDIPortNumber(url: string) {
    for (const [k, v] of Object.entries(DECKLINK_PORT_SETTINGS)) {
        if (v.source!.url == url) {
            return k as DecklinkPort;
        }
    }

    return null;
}
