import "./ProgressBar.css";

export interface ProgressBarProps {
    value: number;
    max?: number;
}

function ProgressBar(inProps: ProgressBarProps) {
    const props = {max: 1, ...inProps};

    return (
        <div className="progressBarContainer">
            <div className="progressBarBar" style={{"width": ((props.value / props.max) * 100) + "%"}}></div>
        </div>
    );
}

export default ProgressBar;
