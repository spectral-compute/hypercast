import {Api} from "./api/Api";
import {MachineInfo, PortConnector} from "./api/Hardware";
import {StreamingConfig} from "./api/Config";
import {DecklinkPort, DECKLINK_PORTS_ORDERED } from "./Constants";



export class AppCtx {
    api: Api = new Api();

    // The server's config object.
    loadedConfiguration!: StreamingConfig;

    // The current status of the hardware.
    machineInfo: MachineInfo = {
        isStreaming: false,
        inputPorts: {
            ["1"]: {
                name: "SDI 1",
                connector: PortConnector.SDI,
                connectedMediaInfo: {
                    video: {
                        height: 1080,
                        width: 1920,
                        frameRate: [30, 1]
                    },
                    audio: {
                        sampleRate: 1000
                    }
                }
            },
            ["2"]: {
                name: "SDI 2",
                connector: PortConnector.SDI,
            },
            ["3"]: {
                name: "SDI 3",
                connector: PortConnector.SDI,
            },
            ["4"]: {
                name: "SDI 4",
                connector: PortConnector.SDI,
            },
        }
    };

    loadConfig = async() => {
        this.loadedConfiguration = await this.api.loadConfig();
        await this.probeSDIPorts();
    };
    saveConfig = async(cfg: StreamingConfig) => {
        this.loadedConfiguration = await this.api.applyConfig(cfg);
    };

    probeSDIPorts = async() => {
        const infos = await this.api.probe(
            DECKLINK_PORTS_ORDERED
        );

        for (let i = 1; i <= DECKLINK_PORTS_ORDERED.length; i++) {
            const o = this.machineInfo.inputPorts[String(i) as DecklinkPort];
            o.connectedMediaInfo = infos[i - 1]!;

            // Strictly speaking, `infos[i-1]` being null means "port doesn't exist", and the other case means there's
            // nothing plugged into it.
            if (!infos[i - 1] || (!o.connectedMediaInfo.video && !o.connectedMediaInfo.audio)) {
                delete o.connectedMediaInfo;
            }
        }
    };

    constructor() {
    }
}
