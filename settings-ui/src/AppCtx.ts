import {Api, IS_SELF_TEST} from "./api/Api";
import {MachineInfo, PortConnector} from "./api/Hardware";
import {StreamingConfig} from "./api/Config";
import {DecklinkPort, DECKLINK_PORTS_ORDERED, DECKLINK_PORT_SETTINGS } from "./Constants";
import {makeObservable, observable} from "mobx";
import {InputPortStatus} from "./App";
import {fuzzyMatch, FuzzyMatchResult} from "./Fuzzify";



export class AppCtx {
    api: Api = new Api();

    // The server's config object.
    loadedConfiguration!: StreamingConfig;

    // The current status of the hardware.
    @observable
    machineInfo: MachineInfo = {
        isStreaming: false,
        inputPorts: {
            ["1"]: {
                name: "SDI 1",
                connector: PortConnector.SDI,
                connectedMediaInfo: {
                    inUse: false,
                    video: {
                        frameRate: [1, 1],
                        width: 3840,
                        height: 2160
                    },
                    audio: {
                        sampleRate: 1
                    }
                }
            },
            ["2"]: {
                name: "SDI 2",
                connector: PortConnector.SDI,
                connectedMediaInfo: {
                    inUse: false,
                    video: {
                        frameRate: [1, 1],
                        width: 3840,
                        height: 2160
                    },
                    audio: {
                        sampleRate: 1
                    }
                }
            },
            ["3"]: {
                name: "SDI 3",
                connector: PortConnector.SDI,
                connectedMediaInfo: {
                    inUse: false,
                    video: {
                        frameRate: [1, 1],
                        width: 3840,
                        height: 2160
                    },
                    audio: {
                        sampleRate: 1
                    }
                }
            },
            ["4"]: {
                name: "SDI 4",
                connector: PortConnector.SDI,
                connectedMediaInfo: {
                    inUse: false,
                    video: {
                        frameRate: [1, 1],
                        width: 3840,
                        height: 2160
                    },
                    audio: {
                        sampleRate: 1
                    }
                }
            },
        }
    };

    // Find the lowest-numbered input port with something plugged into it and not in use.
    getAvailableInputPort(): DecklinkPort | undefined {
        const ps = this.findUnusedPorts();
        if (ps.length > 0) {
            return ps[0];
        }

        return undefined;
    }

    getPortStatus(k: DecklinkPort): InputPortStatus | string {
        const p = this.machineInfo.inputPorts[k]!;

        if (p!.connectedMediaInfo == null) {
            return InputPortStatus.DISCONNECTED;
        }

        for (const [k2, c] of Object.entries(this.loadedConfiguration.channels)) {
            if (fuzzyMatch(c, DECKLINK_PORT_SETTINGS[k]) == FuzzyMatchResult.MATCH) {
                return k2;
            }
        }

        return InputPortStatus.AVAILABLE;
    }

    findUnusedPorts(): DecklinkPort[] {
        let out = [];
        for (const p of DECKLINK_PORTS_ORDERED) {
            if (this.getPortStatus(p) == InputPortStatus.AVAILABLE) {
                out.push(p);
            }
        }

        return out as DecklinkPort[];
    }

    loadConfig = async() => {
        this.loadedConfiguration = await this.api.loadConfig();
        if (!this.loadedConfiguration.channels) {
            this.loadedConfiguration.channels = {}; // The rest of the UI expects this.
        }
    };
    saveConfig = async(cfg: StreamingConfig) => {
        this.loadedConfiguration = await this.api.applyConfig(cfg);
    };

    isSelfTest() {
        return IS_SELF_TEST;
    }

    constructor() {
        makeObservable(this);
    }
}
