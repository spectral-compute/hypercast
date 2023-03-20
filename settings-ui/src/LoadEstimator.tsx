import './LoadEstimator.css';
import ProgressBar from "./ProgressBar";

export interface LoadEstimatorProps {
    // Fractional compute load estimate.
    compute: number;

    // Estimated local bandwidth usage.
    localBandwidth: number;
}

function LoadEstimator(props: LoadEstimatorProps) {
    return (<div className="loadEstContainer">
        <ProgressBar value={props.compute}></ProgressBar>
        <div>
            {props.localBandwidth}
        </div>
    </div>);
}

export default LoadEstimator;
