import Player from "../../components/Player";
import {Player as PlayerMain} from "live-video-streamer-client";
import {PerformanceChart} from "../../performanceChart";
import {useCallback, useEffect, useRef} from "react";
import { useSearchParams } from 'react-router-dom';


export default function LatencyDemo() {
    const latencyRef = useRef<HTMLCanvasElement | null>(null);
    const rateRef = useRef<HTMLCanvasElement | null>(null);
    const performanceChart = useRef<PerformanceChart | null>(null);

    useEffect(() => {
        if (!performanceChart.current) {
            performanceChart.current = new PerformanceChart(latencyRef.current!, rateRef.current!);
        }
        return () => {
            if (performanceChart.current) {
                performanceChart.current.destroy();
                performanceChart.current = null;
            }
        };
    }, []);

    const [searchParams, _] = useSearchParams();
    let serverUrl = searchParams.get("s")  ?? process.env["REACT_APP_STREAM_SERVER"]!;
    console.log("Connecting to server: " + serverUrl);

    const plugInGraphMaker = useCallback((player: PlayerMain) => {
        player.setDebugHandler(performanceChart.current);
    }, []);
    const unplugGraphMaker = useCallback((player: PlayerMain) => {
        player.setDebugHandler(null);
    }, []);

    return <>
        <h2>Ultra Low Latency</h2>
        <Player
            server={serverUrl}
            onInitialisation={plugInGraphMaker}
            onDismount={unplugGraphMaker}
        />
        <div className="timeline" >
            <canvas id="latency_timeline" className="timeline-canvas" ref={latencyRef} height="300px" />
            <canvas id="dlrate_timeline" className="timeline-canvas" ref={rateRef} height="300px" />
        </div>
    </>;
}
