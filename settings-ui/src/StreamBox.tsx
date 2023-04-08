import './StreamBox.sass';
import SecondaryBox from "./SecondaryBox";
import {AudioCodec, AudioVariant, Channel, FrameRate, FrameRateCfg, StreamVariantConfig} from "./api/Config";
import PortStatus from "./PortStatus";
import {useContext} from "react";
import {AppContext} from "./index";
import {ReactComponent as AudioIcon} from "./assets/icons/volume-2.svg";
import {ReactComponent as NoAudioIcon} from "./assets/icons/volume-x.svg";
import {ReactComponent as LatencyIcon} from "./assets/icons/clock.svg";
import {ReactComponent as FrameRateIcon} from "./assets/icons/activity.svg";
import {inputUrlToSDIPortNumber} from "./api/Hardware";
import {ReactComponent as FileIcon} from "./assets/icons/file.svg";
import BoxBtn from "./components/BoxBtn";
import { observer } from 'mobx-react-lite';

export interface StreamBoxProps {
    name: string;
    config: Channel;
    onClick: () => void;
}

export function prettyPrintResolution(w: number, h: number) {
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
        return w + "\nx\n" + h;
    // }
}

function renderAudioStatus(audioCfg: AudioVariant) {
    const hasAudio = audioCfg.codec != AudioCodec.none;
    const icon = hasAudio ? <AudioIcon/> : <NoAudioIcon/>;

    let label = hasAudio ? audioCfg.bitrate + "k" : "None";

    return <BoxBtn label={label} size={5}>
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
            <BoxBtn label={prettyPrintResolution(videoCfg.width, videoCfg.height)} size={5}>
            </BoxBtn>
            <BoxBtn label={prettyPrintFramerate(videoCfg.frameRate)} size={5}>
                <FrameRateIcon/>
            </BoxBtn>

            {renderAudioStatus(audioCfg)}

            <BoxBtn label={prettyPrintTargetLatency(variantCfg.targetLatency)} size={5}>
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

export default observer((props: StreamBoxProps) => {
    const appCtx = useContext(AppContext);
    const chan = props.config;

    const variantSequence = sortVariants(chan);
    const whichSDI = inputUrlToSDIPortNumber(chan.source.url)!;

    return <SecondaryBox>
        <div className="streamInfo" onClick={props.onClick}>
            <div className="sboxHeader">
                <span>{props.name.slice(5)}</span>
                {whichSDI != null ?
                <PortStatus
                    shortLabel={true}
                    size={1}
                    desc={appCtx.machineInfo.inputPorts[whichSDI]!}
                    port={whichSDI}
                ></PortStatus>
                : <FileIcon/>}
            </div>

            {variantSequence.map(
                (p, i) => renderVariant(p, i)
            )}
        </div>
    </SecondaryBox>;
});
