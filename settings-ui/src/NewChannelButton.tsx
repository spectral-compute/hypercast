import './NewChannelButton.sass';
import {ReactComponent as Plus} from "./assets/icons/plus-circle.svg";

export interface NewChannelButtonProps {
    clicked: () => void;
    label?: string;
}


export default (props: NewChannelButtonProps) => {
    props = {
        label: "New Channel",
        ...props
    };
    return <div className="newChannelBtn" onClick={props.clicked}>
        <span>{props.label}</span>
        <Plus width="2em" height="2em"/>
    </div>;
};
