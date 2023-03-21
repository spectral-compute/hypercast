import './StreamBox.sass';
import SecondaryBox from "./SecondaryBox";

export interface StreamBoxProps {
}

function StreamBox() {
    return <SecondaryBox>
        <div className="streamInfo">
            <div className="sboxHeader">
                <span>Stream 1</span>
                <span>LIVE</span>
            </div>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
            <span>Resolution:</span><span>1920x1080</span>
        </div>
    </SecondaryBox>;
}

export default StreamBox;
