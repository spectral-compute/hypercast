import React, {useEffect, useRef} from "react";
import createPlayer, {Player as PlayerMain, PlayerOptions} from "live-video-streamer-client";

import "./Player.scss";

export interface PlayerProps extends PlayerOptions {
    sourceURL: string;
}

export default function Player(props: PlayerProps): React.ReactElement<HTMLDivElement> {
    const containerRef = useRef<HTMLDivElement | null>(null);
    const playerRef = useRef<PlayerMain | null>(null);
    const isInitialised = useRef(false);

    const {sourceURL, ...options} = props;

    useEffect(() => {
        // Make sure Video.js player is only initialized once
        if (!playerRef.current) {
            playerRef.current = createPlayer(sourceURL, containerRef.current!, {
                onInitialisation: (player) => {
                    options.onInitialisation?.(player);
                    isInitialised.current = true;
                },
                ...options
            });
        } else {
            // TODO respond to prop changes here, e.g. changing the stream on sourceURL change...
        }
    }, [sourceURL, options]);

    // Stop the player when the component unmounts
    React.useEffect(() => {
        const player = playerRef.current;
        const container = containerRef.current;
        return () => {
            if (player) {
                if (isInitialised.current) {
                    player.stop();
                }
                playerRef.current = null;
            }
            if (container) {
                container.innerHTML = "";
            }
        };
    }, []);

    return <div className="video-container" ref={containerRef} />;
}
