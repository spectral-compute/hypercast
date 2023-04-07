import {Link} from "react-router-dom";

import {PACKAGE_PLAYER} from "../../variables";
import StyledCode from "../../components/StyledCode";


export default function VanillaJSHowTo() {
    // TODO: the example is awful and the text is too vague
    // TODO: styling the text

    const code = `\
import createPlayer from "${PACKAGE_PLAYER}";

/* ... Acquire sourceURL */
/* ... Acquire containerEl */
/* ... Acquire options */

const player = createPlayer(sourceURL, containerEl, options);

/* The player is present in the page now and can be controlled by the user or programmatically. */
`;

    return <>
        <h2>Using the Player from pure JavaScript</h2>
        <p>
            The <code>{PACKAGE_PLAYER}</code> npm package provides a <code>createPlayer</code> function that can
            populate a given DOM element with a player.
            The function returns a <code>Player</code> object that can be used
            to control the player programmatically.
        </p>
        <p>
            This example shows how to use that function in practice.
            The player can also be <Link to="/how-to/styling">styled</Link>.
        </p>
        <StyledCode code={code}/>
    </>;
}
