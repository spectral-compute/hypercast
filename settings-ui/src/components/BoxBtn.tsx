import "./BoxBtn.sass";
import {CSSProperties} from "react";

export interface BoxBtnProps {
    active?: boolean;
    disabled?: boolean;
    label: string;
    size?: number;
    visible?: boolean;
    children?: any;
    onClick?: () => void;
}

function getStyle(props: BoxBtnProps): CSSProperties {
    return {
        width: props.size + "em",
        height: props.size + "em",
        visibility: props.visible! ? "visible" : "hidden"
    };
}

function getClasses(props: BoxBtnProps) {
    let classes = ["boxBtn"];
    if (props.active) {
        classes.push("active");
    }
    if (props.disabled) {
        classes.push("disabled");
    }
    if (props.label.length > 8) {
        classes.push("sizeHack");
    }

    return classes.join(" ");
}

function BoxBtn(p: BoxBtnProps) {
    const props = {
        onClick: () => {},
        visible: true,
        size: 5,
        ...p
    };

    return <div
        className={getClasses(props)}
        style={getStyle(props)}
        onClick={props.onClick}
    >
        {props.children ? props.children : null}
        <p className="boxBtnLabel">{props.label}</p>
    </div>;
}

export default BoxBtn;
