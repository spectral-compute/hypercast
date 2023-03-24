import React, {useEffect, useRef} from "react";
import createPlayer, {PlayerOptions} from "live-video-streamer-client";

import "./Player.scss";

export type PlayerProps = Omit<PlayerOptions, "container">;

let loading = false;

export default function Player(props: PlayerProps): React.ReactElement<HTMLDivElement> {
    const containerRef = useRef<HTMLDivElement | null>(null);
    const videoRef = useRef<HTMLElement | null>(null);

    useEffect(() => {
        if (videoRef.current === null && !loading) {
            loading = true;
            createPlayer({container: containerRef.current!, ...props})
                .then((videoNode) => {
                    videoRef.current = videoNode;
                    loading = false;
                })
                .catch((error) => {
                    console.error(error);
                    loading = false;
                });
        }
    }, [props]);

    return <div className="video-container" ref={containerRef} />;
}
