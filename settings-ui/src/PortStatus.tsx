import {ReactComponent as SDI} from "./assets/SDI.svg";
import {PortDescriptor} from "./modelling/Hardware";
import './PortStatus.css';

export interface PortStatusProps {
    desc: PortDescriptor;
}

function PortStatus(props: PortStatusProps) {
    return (
        <div className="portStatus">
            <h3>{props.desc.name}</h3>
            <SDI height="3em" width="3em"></SDI>
        </div>
    );
}

export default PortStatus;
