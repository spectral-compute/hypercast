import {Api} from "./api/Api";
// import {Config} from "./api/ServerConfigSpec";
import {MachineInfo, PortConnector} from "./modelling/Hardware";


export class AppCtx {
    api: Api = new Api();

    // The server's config object.
    // loadedConfiguration: Config;

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
}
