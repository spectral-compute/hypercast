import './ChannelConfigModal.sass';
import Modal from "./Modal";
import {ReactComponent as SDI} from "../assets/SDI.svg";
import {useContext, useState} from "react";
import {AppContext} from "../index";
import {AudioCodec, AudioVariant, Channel, StreamVariantConfig, VideoVariant} from "../api/Config";
import {observer} from "mobx-react-lite";
import {inputUrlToSDIPortNumber} from "../api/Hardware";
import {AppCtx} from "../AppCtx";
import {ReactComponent as Cog} from "../assets/icons/settings.svg";
import {DecklinkPort, DECKLINK_PORT_SETTINGS, fuzzyInputMatch, RES_1080p, RES_480p, RES_4k, RES_720p} from "../Constants";
import VariantConfigModal from "./VariantConfigModal";
import BoxBtn from "../components/BoxBtn";
import BoxRadioGroup from '../components/BoxRadioGroup';
import {fuzzyApply} from "../Fuzzify";
import {ReactComponent as Trash} from "../assets/icons/trash-2.svg";

export interface ChannelConfigModalProps {
    onClose: () => void;
    onSave: (name: string, c: Channel) => void;
    onDelete: (name: string) => void;

    showDeleteBtn: boolean;
    channelName: string;
    channel: Channel;
}

function hasStreamWithResolution(channel: Channel, w: number, h: number) {
    for (let i = 0; i < channel.qualities.length; i++) {
        const p = channel.qualities[i]!;
        const v = p.video;
        if (v.width == w && v.height == h) {
            console.log("FOUND");
            console.log(v);
            console.log(i);
            return i;
        }
    }

    return null;
}

function getInputPort(appCtx: AppCtx, channel: Channel) {
    return appCtx.machineInfo.inputPorts[inputUrlToSDIPortNumber(channel.source.url)!]!;
}

function inputIsAtLeast(appCtx: AppCtx, channel: Channel, w: number, h: number) {
    const port = getInputPort(appCtx, channel);
    if (port == null) {
        return true;
    }

    return port.connectedMediaInfo!.height >= h &&
           port.connectedMediaInfo!.width >= w;
}

export function defaultVariantConfig(w: number, h: number): StreamVariantConfig {
    return {
        audio: defaultAudioVariantConfig(),
        video: defaultVideoVariantConfig(w, h),
        targetLatency: 1000
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

// function deleteStreamsReferencing(c: Channel, name: string) {
//     for (const p of Object.keys(c.streams)) {
//         if (c.streams[p]!.audio == name || c.streams[p]!.video == name) {
//             delete c.streams[p];
//         }
//     }
// }
//
// function variantNameFor(w: number, h: number) {
//     return w + "x" + h;
// }

export default observer((props: ChannelConfigModalProps) => {
    const appCtx = useContext(AppContext);

    const [channel, setChannel] = useState<Channel>(props.channel);
    const [variantBeingEdited, setVariantBeingEdited] = useState<number | null>(null);

    function modifyChannel(c: Channel) {
        setChannel({...c});
    }

    function toggleStreamExistence(w: number, h: number) {
        const c = hasStreamWithResolution(channel, w, h);
        if (c != null) {
            channel.qualities.splice(c, 1);

        } else {
            channel.qualities.push(defaultVariantConfig(w, h));
        }

        modifyChannel(channel);
    }

    function openSettingsFor(w: number, h: number) {
        const num = hasStreamWithResolution(channel, w, h)!;
        setVariantBeingEdited(num);
    }

    function saveVariantBeingEdited(cfg: StreamVariantConfig) {
        channel.qualities[variantBeingEdited!] = cfg;
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
            title={"Variant"}
            cfg={channel.qualities[variantBeingEdited]!}

            onCancel={() => setVariantBeingEdited(null)}
            onSave={saveVariantBeingEdited}
        />;
    }

    return <Modal
        endBtn={
            props.showDeleteBtn ?
            <BoxBtn
                label="Delete"
                onClick={() => {
                    props.onDelete(props.channelName);
                    props.onClose();
                }}
            >
                <Trash/>
            </BoxBtn> : null
        }
        title={"Configuring " + props.channelName}
        onClose={props.onClose}
        onSave={saveFn}
    >
        <div className="btnRow">
            <div className="btnDesc">
                <span>Media Source</span>

                What to stream. Ports with nothing connected are disabled.
            </div>

            <BoxRadioGroup<DecklinkPort>
                onSelected={(v) => {
                    setChannel(fuzzyApply(channel, DECKLINK_PORT_SETTINGS[v]));
                }}
                selectedItem={fuzzyInputMatch(channel)}
                items={
                    Object.keys(DECKLINK_PORT_SETTINGS).map((i) => {
                        const k = i as DecklinkPort;
                        const p = appCtx.machineInfo.inputPorts[k]!;
                        return {
                            value: k,
                            label: "SDI " + i,
                            disabled: p!.connectedMediaInfo == null,
                            children: <SDI height={"3em"} width={"3em"}></SDI>
                        };
                    })
                }
            ></BoxRadioGroup>
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
