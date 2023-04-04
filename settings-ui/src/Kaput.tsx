import {ReactComponent as Sadface} from "./assets/icons/frown.svg";
import {ReactComponent as Logo} from "./assets/logo.svg";
import "./Kaput.sass";

export interface KaputProps {
    message?: string;
}

export default (props: KaputProps) => {
    const happy = props.message == "Loading...";

    return <div className="kaput">
        {happy ? <Logo className="happy" width="8em" height="8em"/> : <Sadface width="5em" height="5em"/>}
        {props.message ?? "A fatal error occurred :("}
    </div>;
};
