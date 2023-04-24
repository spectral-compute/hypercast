import React, {useEffect, useRef} from "react";
import {createPlayer, Player as PlayerMain, CreatePlayerOptions} from "live-video-streamer-client";

import "./Player.scss";

export interface PlayerProps extends CreatePlayerOptions {
    sourceURL: string;
    onDismount?: (player: PlayerMain) => void;
}

export default function Player(props: PlayerProps): React.ReactElement<HTMLDivElement> {
    const containerRef = useRef<HTMLDivElement | null>(null);
    const playerRef = useRef<Promise<PlayerMain> | null>(null);

    const {sourceURL, onDismount, ...options} = props;

    // This fires on any prop change. That's why we do dismount cleanup in a separate effect.
    useEffect(() => {
        // Make sure Video.js player is only initialized once
        if (!playerRef.current) {
            playerRef.current = createPlayer(sourceURL, containerRef.current!, {
                ...options
            });
            console.debug("RISE player initialized");
        } else {
            // TODO respond to prop changes here, e.g. changing the stream on sourceURL change...
        }
    }, [sourceURL, options]);

    // Stop the player when the component unmounts
    React.useEffect(() => {
        const player = playerRef.current;
        const container = containerRef.current;
        console.debug("RISE player container mounted"); // KILLME
        return () => {
            if (container) {
                container.replaceChildren();
                console.debug("RISE player container unmounted"); // KILLME
            }
            if (player) {
                player.then((p) => {
                    p.stop();
                    if (onDismount) {
                        onDismount(p);
                    }
                }).catch((e) => {
                    console.error("RISE player failed to initialize:"); // KILLME
                    e instanceof Error && console.error(e.message);
                });
                playerRef.current = null;
            }
        };
    }, []);

    return <div className="video-container" ref={containerRef} />;
}
