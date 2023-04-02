import './StreamBox.sass';
import SecondaryBox from "./SecondaryBox";
import {AudioCodec, AudioVariant, Channel, FrameRate, FrameRateCfg, StreamVariantConfig} from "./api/Config";
import PortStatus from "./PortStatus";
import {useContext} from "react";
import {AppContext} from "./index";
import {ReactComponent as VideoIcon} from "./assets/icons/image.svg";
import {ReactComponent as AudioIcon} from "./assets/icons/volume-2.svg";
import {ReactComponent as NoAudioIcon} from "./assets/icons/volume-x.svg";
import {ReactComponent as LatencyIcon} from "./assets/icons/clock.svg";
import {ReactComponent as FrameRateIcon} from "./assets/icons/activity.svg";
import {inputUrlToSDIPortNumber} from "./api/Hardware";
import {ReactComponent as FileIcon} from "./assets/icons/file.svg";
import BoxBtn from "./components/BoxBtn";

export interface StreamBoxProps {
    name: string;
    config: Channel;
    onClick: () => void;
}

function prettyPrintResolution(w: number, h: number) {
    // if (w == 3840 && h == 2160) {
    //     return "4k";
    // } else if (w == 1920 && h == 1080) {
    //     return "1080p";
    // } else if (w == 1280 && h == 720) {
    //     return "720p";
    // } else if (w == 640 && h == 480) {
    //     return "480p";
    // } else if (w == 320 && h == 240) {
    //     return "240p";
    // } else if (w == 192 && h == 144) {
    //     return "144p";
    // } else {
        return w + "x" + h;
    // }
}

function renderAudioStatus(audioCfg: AudioVariant) {
    const hasAudio = audioCfg.codec != AudioCodec.none;
    const icon = hasAudio ? <AudioIcon/> : <NoAudioIcon/>;

    let label = hasAudio ? audioCfg.bitrate + "k" : "None";

    return <BoxBtn label={label} size={4}>
        {icon}
    </BoxBtn>;
}

function prettyPrintTargetLatency(l: number) {
    if (l % 1000 == 0 || l > 1500) {
        return (l / 1000) + "s";
    } else {
        return l + "ms";
    }
}

function prettyPrintFramerate(f: FrameRateCfg | undefined) {
    if (f == null) {
        return "native";
    }

    if (typeof f == "object") {
        const fps = f as FrameRate;
        return (fps.numerator / fps.denominator) + "fps";
    } else {
        return f as string;
    }
}

function renderVariant(variantCfg: StreamVariantConfig, key: number) {
    const videoCfg = variantCfg.video;
    const audioCfg = variantCfg.audio;

    return <div key={key} className="channelInfoBox">
        <div className="variantInfoRow">
            <BoxBtn label={prettyPrintResolution(videoCfg.width, videoCfg.height)} size={4}>
                <VideoIcon/>
            </BoxBtn>
            <BoxBtn label={prettyPrintFramerate(videoCfg.frameRate)} size={4}>
                <FrameRateIcon/>
            </BoxBtn>

            {renderAudioStatus(audioCfg)}

            <BoxBtn label={prettyPrintTargetLatency(variantCfg.targetLatency)} size={4}>
                <LatencyIcon/>
            </BoxBtn>
        </div>
    </div>;
}

function countPixels(q: StreamVariantConfig) {
    const vid = q.video;
    return vid.width * vid.height;
}

function sortVariants(chan: Channel): StreamVariantConfig[] {
    return chan.qualities.sort((x, y) => {
        return countPixels(x) - countPixels(y);
    });
}

function StreamBox(props: StreamBoxProps) {
    const appCtx = useContext(AppContext);
    const chan = props.config;

    const variantSequence = sortVariants(chan);
    const whichSDI = inputUrlToSDIPortNumber(chan.source.url)!;

    return <SecondaryBox>
        <div className="streamInfo" onClick={props.onClick}>
            <div className="sboxHeader">
                <span>{props.name}</span>
                {whichSDI != null ?
                <PortStatus
                    shortLabel={true}
                    size={1}
                    desc={appCtx.machineInfo.inputPorts[whichSDI]!}
                    connected={true}
                ></PortStatus>
                : <FileIcon/>}
            </div>

            {variantSequence.map(
                (p, i) => renderVariant(p, i)
            )}
        </div>
    </SecondaryBox>;
}

export default StreamBox;
//
// Perhaps, for bitrate: Low, medium, high (I'm going to call this "quality") - and map this to the CRF setting (this will cause the bitrates to be set automatically) and audio bitrate.
// Perhaps for latency: 1s, 2s, 3s, 5s, 10s (with the first ones greyed out according to a function that I specify of the quality, frame rate, and resolution).
// Perhaps for resolution: 4k, 1080p, 720p, 360p (you probably want to query the source to exclude resolutions that are higher than the source).
// Perhaps for frame rate: full, half+. Or whatever you want to call it. The idea of full is that you get 30fps if the source is 30fps, and 60 fps if it's 60 fps. The idea of half+ is that you get 30fps for a source frame rate of both 30fps and 60fps (the exact definition is given in the manual). You might want to implement that by querying the source to see what its frame rate is, and only offering the choice between full and half if it's sensible to do so (the manual gives a definition of half+ which is intended to correspond to a definition of whether it's sensible to use half framerate).
