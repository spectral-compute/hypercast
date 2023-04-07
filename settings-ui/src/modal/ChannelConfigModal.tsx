import './ChannelConfigModal.sass';
import Modal from "./Modal";
import {ReactComponent as SDI} from "../assets/SDI.svg";
import {useContext, useState} from "react";
import {AppContext} from "../index";
import {Channel, StreamVariantConfig} from "../api/Config";
import {observer} from "mobx-react-lite";
import {inputUrlToSDIPortNumber} from "../api/Hardware";
import {AppCtx} from "../AppCtx";
import {ReactComponent as Cog} from "../assets/icons/settings.svg";
import {
    DECKLINK_PORT_SETTINGS,
    DecklinkPort,
    defaultVariantConfig,
    fuzzyInputMatch,
    RES_1080p,
    RES_480p,
    RES_4k,
    RES_720p
} from "../Constants";
import VariantConfigModal from "./VariantConfigModal";
import BoxBtn from "../components/BoxBtn";
import BoxRadioGroup from '../components/BoxRadioGroup';
import {fuzzyApply} from "../Fuzzify";
import {ReactComponent as Trash} from "../assets/icons/trash-2.svg";
import {getPortStatus, InputPortStatus} from "../App";

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
            return i;
        }
    }

    return null;
}

function getInputPort(appCtx: AppCtx, channel: Channel) {
    if (!channel.source) {
        return null; // makeDefaultChannel omits the source if given null, which happens if no port is connected.
    }
    return appCtx.machineInfo.inputPorts[inputUrlToSDIPortNumber(channel.source.url)!]!;
}

function inputIsAtLeast(appCtx: AppCtx, channel: Channel, w: number, h: number) {
    const port = getInputPort(appCtx, channel);
    if (port == null) {
        return false;
    }

    // Nothing connected.
    if (!port.connectedMediaInfo || !port.connectedMediaInfo.video) {
        return false;
    }

    return port.connectedMediaInfo!.video.height >= h &&
           port.connectedMediaInfo!.video.width >= w;
}

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
        title={"Configuring " + props.channelName.slice(5)}
        onClose={props.onClose}
        onSave={saveFn}
    >
        <div className="btnRow">
            <div className="btnDesc">
                <span>Media Source</span>

                What to stream. Ports with nothing connected, or which are in use by another channel, are disabled.
            </div>

            <BoxRadioGroup<DecklinkPort>
                onSelected={(v) => {
                    setChannel(fuzzyApply(channel, DECKLINK_PORT_SETTINGS[v]));
                }}
                selectedItem={fuzzyInputMatch(channel)}
                items={
                    Object.keys(DECKLINK_PORT_SETTINGS).map((i) => {
                        const k = i as DecklinkPort;
                        const status = getPortStatus(appCtx, k);
                        let disabled = status != InputPortStatus.AVAILABLE && status != props.channelName;

                        return {
                            value: k,
                            label: "SDI " + i,
                            disabled,
                            children: <SDI height="3em" width="3em"/>
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
