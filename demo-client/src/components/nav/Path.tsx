import {Link, useLocation} from "react-router-dom";

import "./Path.scss";
import {Fragment} from "react";


/*
    This is a configuration object for the Path component that maps locations to page names.
    If the location is not known to the component, the component will not be rendered.
    This means that this list should be kept updated with page names.
    It is easier than creating this component dynamically using nesting with <Outlet/> components.

    However, this can be improved if it becomes a problem.
*/

const CONFIGURATION: Record<string, string> = {
    "/": "Home",
    "/demo": "Demo",
    "/demo/latency": "Latency",
    "/demo/styling": "Styling",
    "/how-to": "How-to",
    "/how-to/react": "React",
    "/how-to/styling": "Styling",
    "/how-to/vanilla-js": "Vanilla JS",
};

export default function () {
    const location = useLocation();

    // Find all matches of the current location in the configuration object and sort them by length, shortest first.
    // These matches will be used to construct a path to the current location where every parent can be navigated to.

    const matches = Object.keys(CONFIGURATION);
    const sortedMatches = matches
        .filter((match) => location.pathname.startsWith(match))
        .sort((a, b) => a.length - b.length);

    // Construct the path to the current location.

    const path = sortedMatches.map((match) => {
        const name = CONFIGURATION[match];
        return <Fragment key={match}>
            <span className="separator">/</span>
            <Link className="path" to={match}>{name}</Link>
        </Fragment>;
    });

    return <nav className="path-container">{path}</nav>;
}
