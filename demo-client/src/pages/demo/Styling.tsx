import {useEffect, useRef} from "react";
import {Link} from "react-router-dom";

import Player from "../../components/Player";
import StyledCode from "../../components/StyledCode";


export default function StylingDemo() {
    const playerContainer = useRef<HTMLDivElement | null>(null);

    const style = `\
.video-container {
    background-color: pink;
}

.video-container .video-controls {
    background-color: lightblue;
}

.video-controls button,
.video-controls select {
    font-size: 2em;
}

.video-controls {
    grid-template-areas: "fullscreen start-stop . mute angle quality" !important;
}`;

    useEffect(() => {
        const p = playerContainer.current;
        if (p) {
            p.setAttribute("style", style);
        }
    }, []);

    return <>
        <h2>Styling Demonstration</h2>
        <p>
            The player on this page is styled using the provided styles.
            See more information about how to style the player in the <Link to="/how-to/styling">Styling How-to</Link>.
        </p>
        <StyledCode language="css" code={style}/>
        <p>
            There are many open possiblities for styling the player.
            For example, as the provided controls are arranged using a grid, they can easily be reordered and adjusted.
        </p>

        <style>
            {style}
        </style>
        <Player
            sourceURL={process.env["REACT_APP_INFO_URL"]!}
        />
    </>;
}
