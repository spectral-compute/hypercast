import {useEffect, useState} from "react";

import * as lvsc from "live-video-streamer-client";
import StateButton from "./StateButton";


/* Variables we expect to receive from the build environment. */
declare namespace process {
    let env: {
        REACT_APP_INFO_URL: string | undefined;
    };
}


// This pretends to be a player but actually renders to a `#video` outside of react land for now.
export default function Player(): React.ReactElement<HTMLDivElement> {
    const [player, setPlayer] = useState<lvsc.Player | null>(null);

    useEffect(() => {
        const init = async (): Promise<void> => {
            /* Get the video element. */
            const videoEl = document.getElementById("video") as HTMLVideoElement;
            /* Create the player object. */
            const lvscPlayer = new lvsc.Player(process.env.REACT_APP_INFO_URL ?? null, videoEl);

            // useEffect is fired asynchronously, so we should update the state properly.
            // useEffect won't be re-run though, because there are no dependencies for
            setPlayer(lvscPlayer);

            /* Initialize and start the player. */
            await lvscPlayer.init();
            lvscPlayer.start();
        };

        init()
            .catch(console.error);
    }, []);

    return <div className="player">
        <div className="video" style={{border: "1px solid black"}}>
            "Video go here"
        </div>
        <div className="controls">
            <StateButton
                id="playback-control"
                states={[
                    {
                        name: "Stop",
                        fn: player?.stop
                    }, {
                        name: "Start",
                        fn: player?.start
                    }
                ]}
            />
            <StateButton
                id="mute-control"
                states={[
                    {
                        name: "Unmute",
                        fn: player?.unmute
                    }, {
                        name: "Mute",
                        fn: player?.mute
                    }
                ]}
            />
        </div>
    </div>;
}
