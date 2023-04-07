import {Link} from "react-router-dom";

import {PACKAGE_PLAYER, PACKAGE_PLAYER_REACT} from "../../variables";
import StyledTypescript from "../../components/StyledTypescript";


export default function ReactHowTo() {
    // TODO: styling the text

    const code = `\
import Player from "${PACKAGE_PLAYER_REACT}";

export default function PageWithPlayer(props: {sourceURL: string}) {
    // Let's add some extra handlers to the player
    const init = useCallback((player: Player) => {
        /* Your code */
    }, []);
    const dismount = useCallback((player: Player) => {
        /* Your code */
    }, []);

    return <>
        <p>Showing stream from {props.sourceURL}</p>
        <Player
            sourceURL={props.sourceURL}
            onInitialisation={init}
            onDismount={dismount}
        />
    </>;
}`;

    return <>
        <h2>Using the Player from React</h2>
        <p>
            The <code>{PACKAGE_PLAYER_REACT}</code> npm package provides a simple interface to use the player
            in a React application.
            The <code>Player</code> component is a wrapper around the <code>Player</code> class from
            the <code>{PACKAGE_PLAYER}</code> package.
        </p>
        <p>
            This example shows how to use the <code>Player</code> component.
            The <code>sourceURL</code> prop is the URL of the stream to play, the component will handle the rest.
            The component can be customized with callbacks and <Link to="/how-to/styling">styled</Link>.
        </p>
        <StyledTypescript code={code}/>
    </>;
}
