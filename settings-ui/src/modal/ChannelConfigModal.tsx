import './ChannelConfigModal.sass';
import Modal from "./Modal";
import PortStatus from "../PortStatus";
import {useContext, useState} from "react";
import {AppContext} from "../index";
import {AudioCodec, AudioVariant, Channel, StreamVariantConfig, VideoVariant} from "../api/Config";
import {observer} from "mobx-react-lite";
import {inputUrlToSDIPortNumber} from "../api/Hardware";
import {AppCtx} from "../AppCtx";
import {ReactComponent as Cog} from "../assets/icons/settings.svg";
import {RES_1080p, RES_480p, RES_4k, RES_720p} from "../Constants";
import VariantConfigModal from "./VariantConfigModal";
import BoxBtn from "../components/BoxBtn";

export interface ChannelConfigModalProps {
    onClose: () => void;
    onSave: (name: string, c: Channel) => void;

    channelName: string;
    channel: Channel;
}

function hasStreamWithResolution(channel: Channel, w: number, h: number) {
    for (const p of Object.entries(channel.videoVariants)) {
        const v = p[1];
        if (v.width == w && v.height == h) {
            return p[0];
        }
    }

    return null;
}

function getInputPort(appCtx: AppCtx, channel: Channel) {
    return appCtx.machineInfo.inputPorts[inputUrlToSDIPortNumber(channel.videoSource.url)!]!;
}

function inputIsAtLeast(appCtx: AppCtx, channel: Channel, w: number, h: number) {
    const port = getInputPort(appCtx, channel);

    return port.connectedMediaInfo!.height >= h &&
           port.connectedMediaInfo!.width >= w;
}

export function defaultVariantConfig(name: string): StreamVariantConfig {
    return {
        audio: name,
        video: name,
        targetLatencyMs: 1000
    };
}

export function defaultAudioVariantConfig(): AudioVariant {
    return {
        codec: AudioCodec.aac,
        bitrate: 1000
    };
}

export function defaultVideoVariantConfig(w: number, h: number): VideoVariant {
    return {
        width: w,
        height: h
    };
}

function deleteStreamsReferencing(c: Channel, name: string) {
    for (const p of Object.keys(c.streams)) {
        if (c.streams[p]!.audio == name || c.streams[p]!.video == name) {
            delete c.streams[p];
        }
    }
}

function variantNameFor(w: number, h: number) {
    return w + "x" + h;
}

export default observer((props: ChannelConfigModalProps) => {
    const appCtx = useContext(AppContext);

    const [channel, setChannel] = useState<Channel>(props.channel);
    const [variantBeingEdited, setVariantBeingEdited] = useState<[VideoVariant, AudioVariant, StreamVariantConfig, string] | null>(null);

    function modifyChannel(c: Channel) {
        setChannel({...c});
    }

    function toggleStreamExistence(w: number, h: number) {
        const c = hasStreamWithResolution(channel, w, h);
        const canonicalName = variantNameFor(w, h);

        if (c) {
            delete channel.videoVariants[c];
            delete channel.audioVariants[c];

            deleteStreamsReferencing(channel, c);
        } else {
            const name = canonicalName;
            channel.videoVariants[name] = defaultVideoVariantConfig(w, h);
            channel.audioVariants[name] = defaultAudioVariantConfig();
            channel.streams[name] = defaultVariantConfig(name);
        }

        modifyChannel(channel);
    }

    function canonicaliseChannel(name: string, w: number, h: number) {
        channel.videoVariants[name] ??= defaultVideoVariantConfig(w, h);
        channel.audioVariants[name] ??= defaultAudioVariantConfig();
        channel.streams[name] ??= defaultVariantConfig(name);
        modifyChannel(channel);
    }

    function openSettingsFor(w: number, h: number) {
        const name = hasStreamWithResolution(channel, w, h)!;
        canonicaliseChannel(name, w, h);
        setVariantBeingEdited([
            channel.videoVariants[name]!,
            channel.audioVariants[name]!,
            channel.streams[name]!,
            name
        ]);
    }

    function saveVariantBeingEdited(video: VideoVariant, audio: AudioVariant, stream: StreamVariantConfig) {
        const n = variantBeingEdited![3];
        channel.videoVariants[n] = video;
        channel.audioVariants[n] = audio;
        channel.streams[n] = stream;
        setVariantBeingEdited(null);
        modifyChannel(channel);
    }

    // Such hacks, very wow.
    const saveFn = () => {
        props.onSave(props.channelName, channel);
        props.onClose();
    };

    if (variantBeingEdited != null) {
        return <VariantConfigModal
            title={variantBeingEdited[3]}
            video={variantBeingEdited![0]}
            audio={variantBeingEdited![1]}
            stream={variantBeingEdited[2]}

            onCancel={() => setVariantBeingEdited(null)}
            onSave={saveVariantBeingEdited}
        />;
    }

    return <Modal
        title={"Configuring " + props.channelName}
        onClose={props.onClose}
        onSave={saveFn}
    >
        <div className="btnRow">
            <div className="btnDesc">
                <span>Media Source</span>

                What to stream. Ports with nothing connected are disabled.
            </div>

            <div className="portList">
                {
                    appCtx.machineInfo.inputPorts.map(p =>
                        <BoxBtn
                            label=""
                            active={getInputPort(appCtx, channel) == p}
                            disabled={p.connectedMediaInfo == null}
                        >
                            <PortStatus connected={p.connectedMediaInfo != null} desc={p}></PortStatus>
                        </BoxBtn>
                    )}
            </div>
        </div>

        <hr/>


        <div className="btnRow">
            <div className="btnDesc">
                <span>Variants</span>
                Select which resolutions this stream should be made available in.
                Each one can be configured separately.
            </div>

            <BoxBtn
                label="4k"
                active={hasStreamWithResolution(channel, ...RES_4k) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_4k)}
                onClick={() => toggleStreamExistence(...RES_4k)}
            ></BoxBtn>
            <BoxBtn
                label="1080p"
                active={hasStreamWithResolution(channel, ...RES_1080p) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_1080p)}
                onClick={() => toggleStreamExistence(...RES_1080p)}
            ></BoxBtn>
            <BoxBtn
                label="720p"
                active={hasStreamWithResolution(channel, ...RES_720p) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_720p)}
                onClick={() => toggleStreamExistence(...RES_720p)}
            ></BoxBtn>
            <BoxBtn
                label="480p"
                active={hasStreamWithResolution(channel, ...RES_480p) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_480p)}
                onClick={() => toggleStreamExistence(...RES_480p)}
            ></BoxBtn>
        </div>

        <div className="btnRow">
            <div className="btnDesc">
                <span></span>
            </div>

            <BoxBtn
                label=""
                visible={hasStreamWithResolution(channel, ...RES_4k) != null}
                onClick={() => openSettingsFor(...RES_4k)}
            ><Cog/></BoxBtn>
            <BoxBtn
                label=""
                visible={hasStreamWithResolution(channel, ...RES_1080p) != null}
                onClick={() => openSettingsFor(...RES_1080p)}
            >
                <Cog/>
            </BoxBtn>
            <BoxBtn
                label=""
                visible={hasStreamWithResolution(channel, ...RES_720p) != null}
                onClick={() => openSettingsFor(...RES_720p)}
            >
                <Cog/>
            </BoxBtn>
            <BoxBtn
                label=""
                visible={hasStreamWithResolution(channel, ...RES_480p) != null}
                onClick={() => openSettingsFor(...RES_480p)}
            >
                <Cog/>
            </BoxBtn>
        </div>
     </Modal>;
});
