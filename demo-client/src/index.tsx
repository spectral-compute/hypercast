import React from "react";
import ReactDOM from "react-dom/client";

import "./index.css";
import Player from "./Player";


ReactDOM.createRoot(document.getElementById("root")!).render(
    <React.StrictMode>
        <Player/>

        <div className="description">
            This is a demo of <a href="https://spectralcompute.co.uk/" target="_blank">Spectral Compute</a>'s ultra
            low-latency video streaming product.
        </div>
    </React.StrictMode>
);
