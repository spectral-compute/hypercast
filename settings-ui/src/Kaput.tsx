import {ReactComponent as Sadface} from "./assets/icons/frown.svg";
import {ReactComponent as Logo} from "./assets/logo.svg";
import "./Kaput.sass";

export interface KaputProps {
    message?: string;
}

function spinner() {
    // Well, this is insane.
    let is = [];
    for (let i = 0; i < 15; i++) {
        is.push(i);
    }

    return <div className="happyContainer">{
        is.map((i) => <Logo className={"happy " + " logo" + i} width="8em" height="8em"/>)
    }</div>;
}

export default (props: KaputProps) => {
    const happy = props.message == "Loading...";

    return <div className="kaput">
        {happy ? spinner() : <Sadface width="5em" height="5em"/>}
        {props.message ?? "A fatal error occurred :("}
    </div>;
};
