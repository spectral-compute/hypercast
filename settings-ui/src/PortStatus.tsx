import {ReactComponent as SDI} from "./assets/SDI.svg";
import {PortDescriptor} from "./api/Hardware";
import './PortStatus.sass';

export interface PortStatusProps {
    desc: PortDescriptor;
    connected: boolean;
}

function PortStatus(props: PortStatusProps) {

    return (
        <div className={"portStatus" + (props.connected ? " connected" : " disconnected")}>
            <h3>{props.desc.name}</h3>
            <SDI height="3em" width="3em"></SDI>
        </div>
    );
}

export default PortStatus;
