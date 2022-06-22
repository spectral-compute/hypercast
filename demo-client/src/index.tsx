import React from "react";
import ReactDOM from "react-dom/client";
import "./index.css";

import * as lvsc from "live-video-streamer-client";

declare namespace process {
    let env: {
        REACT_APP_INFO_URL: string;
    };
}

const player: lvsc.Player = new lvsc.Player(process.env.REACT_APP_INFO_URL,
                                            document.getElementById("video")! as HTMLVideoElement, null);

function App(): React.ReactElement<HTMLDivElement> {
    return (<div></div>);
}

const root = ReactDOM.createRoot(document.getElementById("root")!);
root.render(
    <React.StrictMode>
        <App />
    </React.StrictMode>
);

/* Start. */
async function init(): Promise<void> {
    await player.init();
    player.start();
}
void init();
