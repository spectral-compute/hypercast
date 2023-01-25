import React from "react";
import ReactDOM from "react-dom/client";

import {StateButton} from "./StateButton";
import "./index.css";

import * as lvsc from "live-video-streamer-client";

/* Variables we expect to receive from the build environment. */
declare namespace process {
    let env: {
        REACT_APP_INFO_URL: string | undefined;
    };
}

/* Create the player object. */
const player: lvsc.Player = new lvsc.Player(process.env.REACT_APP_INFO_URL ? process.env.REACT_APP_INFO_URL : null,
                                            document.getElementById("video")! as HTMLVideoElement);

/* Start. */
async function init(): Promise<void> {
    await player.init();
    player.start();

    ReactDOM.createRoot(document.getElementById("controls")!).render(
        <React.StrictMode>
            <div>
                <StateButton states={[
                    {
                        name: "Stop",
                        fn: (): void => {
                            player.stop();
                        }
                    }, {
                        name: "Start",
                        fn: (): void => {
                            player.start();
                        }
                    }
                ]} />
                <StateButton states={[
                    {
                        name: "Unmute",
                        fn: (): void => {
                            player.setMuted(false);
                        }
                    }, {
                        name: "Mute",
                        fn: (): void => {
                            player.setMuted(true);
                        }
                    }
                ]} />
            </div>
        </React.StrictMode>
    );
}
void init();
