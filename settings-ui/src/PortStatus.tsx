import {ReactComponent as SDI} from "./assets/SDI.svg";
import {PortDescriptor} from "./api/Hardware";
import './PortStatus.sass';
import {DecklinkPort} from "./Constants";
import {getPortStatus, InputPortStatus} from "./App";
import {useContext} from "react";
import {AppContext} from "./index";

export interface PortStatusProps {
    desc: PortDescriptor;
    port: DecklinkPort;
    size?: number;
    shortLabel?: boolean;
}

function PortStatus(p: PortStatusProps) {
    const appCtx = useContext(AppContext);
    const status = getPortStatus(appCtx, p.port);
    const props = {
        size: 3,
        ...p
    };

    let label = props.desc.name;
    if (props.shortLabel) {
        label = label.slice(4);
    }

    let className;
    if (status == InputPortStatus.AVAILABLE) {
        className = "connected";
    } else if (status == InputPortStatus.DISCONNECTED) {
        className = "disconnected";
    } else {
        className = "streaming";
    }

    return (
        <div className={"portStatus " + className}>
            <h3>{label}</h3>
            <SDI height={props.size + "em"} width={props.size + "em"}></SDI>
        </div>
    );
}

export default PortStatus;
