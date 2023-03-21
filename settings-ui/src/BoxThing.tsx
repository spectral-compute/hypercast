import './BoxThing.sass';

export interface BoxThingProps {
    title?: string;
    children: any;
}

function BoxThing(props: BoxThingProps) {
    return (
        <div className="boxThing">
            {(props.title ? (<h2>{props.title}</h2>) : null)}

            {props.children}
        </div>
    );
}

export default BoxThing;
