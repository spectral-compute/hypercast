import {AppContext} from "./index";
import PortStatus from "./PortStatus";
import {useContext} from "react";

function PortsStatus() {
    const appCtx = useContext(AppContext);

    return <div className="portStatusList">{
        appCtx.machineInfo.inputPorts.map(p =>
        <PortStatus desc={p}></PortStatus>
    )}</div>;
}

export default PortsStatus;
