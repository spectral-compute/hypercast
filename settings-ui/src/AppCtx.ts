import {Api} from "./api/Api";
import {MachineInfo, PortConnector} from "./api/Hardware";
import {StreamingConfig} from "./api/Config";
import {DecklinkPort, DECKLINK_PORTS_ORDERED, DECKLINK_PORT_SETTINGS } from "./Constants";
import {makeObservable, observable} from "mobx";



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
                connector: PortConnector.SDI
            },
            ["2"]: {
                name: "SDI 2",
                connector: PortConnector.SDI
            },
            ["3"]: {
                name: "SDI 3",
                connector: PortConnector.SDI
            },
            ["4"]: {
                name: "SDI 4",
                connector: PortConnector.SDI
            },
        }
    };

    // Find the lowest-numbered input port with something pugged into it.
    getAvailableInputPort(): DecklinkPort | null {
        for (const [k, v] of Object.entries(this.machineInfo.inputPorts)) {
            if (v.connectedMediaInfo) {
                return k as DecklinkPort;
            }
        }

        return null;
    }

    loadConfig = async() => {
        this.loadedConfiguration = await this.api.loadConfig();
        if (!this.loadedConfiguration.channels) {
            this.loadedConfiguration.channels = {}; // The rest of the UI expects this.
        }

        // We only get here if the config load succeeded, which suggests that now is a good time to
        // start polling the ports, too.
        this.startPollingPorts();
    };
    saveConfig = async(cfg: StreamingConfig) => {
        this.loadedConfiguration = await this.api.applyConfig(cfg);
    };

    probeSDIPorts = async() => {
        const infos = await this.api.probe(
            DECKLINK_PORTS_ORDERED.map(x => DECKLINK_PORT_SETTINGS[x]!.source as MediaSource)
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

    portPollTimer: any = null;
    startPollingPorts() {
        if (this.portPollTimer != null) {
            return;
        }

        this.portPollTimer = setInterval(this.probeSDIPorts, 5000);
    }
    stopPollingPorts() {
        if (this.portPollTimer == null) {
            return;
        }

        clearInterval(this.portPollTimer);
        this.portPollTimer = null;
    }

    constructor() {
        makeObservable(this);
    }
}
