import "./BoxRadioGroup.sass";
import BoxBtn from "./BoxBtn";

export interface BoxRadioEntry<T> {
    value: T;
    label?: string;
    disabled?: boolean;
    children?: any;
}


export interface BoxRadioProps<T> {
    selectedItem: T | null;
    items: BoxRadioEntry<T>[];
    disabled?: boolean;
    onSelected: (item: T) => void;
}

function getClasses<T>(p: BoxRadioProps<T>) {
    let classes = ["boxRadioGroup"];
    if (p.disabled) {
        classes.push("disabled");
    }

    return classes.join(" ");
}

export default function<T>(props: BoxRadioProps<T>) {
    return <div
        className={getClasses(props)}
    >
        {props.items.map(item =>
            <BoxBtn
                label={item.label ?? ""}
                onClick={() => props.onSelected(item.value)}
                active={props.selectedItem == item.value}
                disabled={(props.disabled ?? false) || (item.disabled ?? false)}
            >
                {item.children}
            </BoxBtn>
        )}
    </div>;
}
