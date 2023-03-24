import React from "react";
import ReactDOM from "react-dom/client";
import "./index.css";
import Player from "./Player";


/* Variables we expect to receive from the build environment. */
declare namespace process {
    let env: {
        REACT_APP_INFO_URL: string | undefined;
    };
}

ReactDOM.createRoot(document.getElementById("root")!).render(
    <React.StrictMode>
        <Player sourceURL={process.env.REACT_APP_INFO_URL!}/>

        <div className="description">
            This is a demo of <a href="https://spectralcompute.co.uk/" target="_blank">Spectral Compute</a>'s ultra
            low-latency video streaming product.
        </div>
    </React.StrictMode>
);
