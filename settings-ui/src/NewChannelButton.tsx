import './NewChannelButton.sass';
import {ReactComponent as Plus} from "./assets/icons/plus-circle.svg";

export interface NewChannelButtonProps {
    clicked: () => void;
}


export default (props: NewChannelButtonProps) => {
    return <div className="newChannelBtn" onClick={props.clicked}>
        <span>New Channel</span>
        <Plus width="2em" height="2em"/>
    </div>;
};
