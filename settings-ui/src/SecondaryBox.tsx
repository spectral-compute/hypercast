import './SecondaryBox.sass';

export interface SecondaryBoxProps {
    children: any;
}

function SecondaryBox(props: SecondaryBoxProps) {
    return <div className="secondaryBox">
        {props.children}
     </div>;
}

export default SecondaryBox;
