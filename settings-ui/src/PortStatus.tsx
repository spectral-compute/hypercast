import {ReactComponent as SDI} from "./assets/SDI.svg";
import {PortDescriptor} from "./api/Hardware";
import './PortStatus.sass';

export interface PortStatusProps {
    desc: PortDescriptor;
    connected: boolean;
    size?: number;
    shortLabel?: boolean;
}

function PortStatus(p: PortStatusProps) {
    const props = {
        size: 3,
        ...p
    };

    let label = props.desc.name;
    if (props.shortLabel) {
        label = label.slice(4);
    }

    return (
        <div className={"portStatus" + (props.connected ? " connected" : " disconnected")}>
            <h3>{label}</h3>
            <SDI height={props.size + "em"} width={props.size + "em"}></SDI>
        </div>
    );
}

export default PortStatus;
