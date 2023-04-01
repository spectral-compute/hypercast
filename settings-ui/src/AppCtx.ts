import {Api} from "./api/Api";
import {MachineInfo, PortConnector} from "./api/Hardware";
import {StreamingConfig} from "./api/Config";



export class AppCtx {
    api: Api = new Api();

    // The server's config object.
    loadedConfiguration!: StreamingConfig;

    // The current status of the hardware.
    machineInfo: MachineInfo = {
        isStreaming: false,
        inputPorts: [{
            name: "SDI 1",
            connector: PortConnector.SDI,
            connectedMediaInfo: {
                framerateDenominator: 30,
                framerateNumerator: 1,
                height: 1080,
                width: 1920,
            }
        }, {
            name: "SDI 2",
            connector: PortConnector.SDI,
        }, {
            name: "SDI 3",
            connector: PortConnector.SDI,
        }, {
            name: "SDI 4",
            connector: PortConnector.SDI,
        },
        ]
    };

    loadConfig = async() => {
        this.loadedConfiguration = await this.api.loadConfig();
    };
    saveConfig = async(cfg: StreamingConfig) => {
        this.loadedConfiguration = await this.api.applyConfig(cfg);
    };

    constructor() {
    }
}
