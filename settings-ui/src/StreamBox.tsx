import './StreamBox.sass';
import SecondaryBox from "./SecondaryBox";
import {AudioQuality, Channel, VideoQuality} from "./api/Config";

export interface StreamBoxProps {
    config: Channel;
}

function renderVariant(_video: VideoQuality, _audio: AudioQuality) {
    return <>
        <hr/>
        <div className="variantCtr">
        <div className="variantInfo">
            <h4>Video</h4>
            <span>Resolution: 1920x1080</span>
            <span>Quality: medium</span>
        </div>
        <div className="variantInfo">
            <h4>Audio</h4>
            <span>Quality: medium</span>
            <span>Codec: AAC</span>
        </div>
    </div>
    </>;
}

function StreamBox(props: StreamBoxProps) {
    const chan = props.config;
    return <SecondaryBox>
        <div className="streamInfo">
            <div className="sboxHeader">
                <span>{chan.name}</span>
                <span>LIVE</span>
            </div>

            <div className="channelInfoBox">
                <span>Source: SDI1</span>
                <span>Target Latency: 1s</span>

                {chan.videoQualities.map((vq, i) => renderVariant(vq, chan.audioQualities[i]!))}
            </div>
        </div>
    </SecondaryBox>;
}

export default StreamBox;
//
// Perhaps, for bitrate: Low, medium, high (I'm going to call this "quality") - and map this to the CRF setting (this will cause the bitrates to be set automatically) and audio bitrate.
// Perhaps for latency: 1s, 2s, 3s, 5s, 10s (with the first ones greyed out according to a function that I specify of the quality, frame rate, and resolution).
// Perhaps for resolution: 4k, 1080p, 720p, 360p (you probably want to query the source to exclude resolutions that are higher than the source).
// Perhaps for frame rate: full, half+. Or whatever you want to call it. The idea of full is that you get 30fps if the source is 30fps, and 60 fps if it's 60 fps. The idea of half+ is that you get 30fps for a source frame rate of both 30fps and 60fps (the exact definition is given in the manual). You might want to implement that by querying the source to see what its frame rate is, and only offering the choice between full and half if it's sensible to do so (the manual gives a definition of half+ which is intended to correspond to a definition of whether it's sensible to use half framerate).
