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
    inputPorts: PortDescriptor[];

    // Whether or not streaming is actually turned on.
    isStreaming: boolean;
}
