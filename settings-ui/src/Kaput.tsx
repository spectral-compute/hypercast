import {ReactComponent as Sadface} from "./assets/icons/frown.svg";
import "./Kaput.sass";

export interface KaputProps {
    message?: string;
}

export default (props: KaputProps) => {
    return <div className="kaput">
        <Sadface width="5em" height="5em"></Sadface>
        {props.message ?? "A fatal error occurred :("}
    </div>;
};
