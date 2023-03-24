import React, {useEffect, useRef} from "react";
import createPlayer from "live-video-streamer-client";

export interface PlayerProps {
    sourceURL: string;
}

let loading = false;

export default function Player(props: PlayerProps): React.ReactElement<HTMLDivElement> {
    const containerRef = useRef<HTMLDivElement | null>(null); // TODO rename?
    const videoRef = useRef<HTMLElement | null>(null);

    useEffect(() => {
        if (videoRef.current === null && !loading) {
            loading = true;
            createPlayer({container: containerRef.current!, source: props.sourceURL})
                .then((videoNode) => {
                    videoRef.current = videoNode;
                    loading = false;
                })
                .catch((error) => {
                    console.error(error);
                    loading = false;
                });
        }
    }, [props.sourceURL]);

    return <div className="video-container" ref={containerRef} />;
}
