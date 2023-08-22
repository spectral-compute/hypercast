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
    RES_4k, RES_1080p, RES_720p, RES_360p,
} from "../Constants";
import VariantConfigModal from "./VariantConfigModal";
import BoxBtn from "../components/BoxBtn";
import BoxRadioGroup from '../components/BoxRadioGroup';
import {fuzzyApply} from "../Fuzzify";
import {ReactComponent as Trash} from "../assets/icons/trash-2.svg";
import {InputPortStatus} from "../App";
import { prettyPrintResolution } from '../StreamBox';
import {useTranslation} from "react-i18next";
import Loading from "../Loading";

export interface ChannelConfigModalProps {
    onClose: () => void;
    onSave: (name: string, c: Channel) => void;
    onDelete: (name: string) => void;

    probeifier: any;

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
    const {t} = useTranslation();

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
        if (channel.qualities.length == 0) {
            return;
        }

        props.onSave(props.channelName, channel);
        props.onClose();
    };

    console.log(props.probeifier.isLoading);

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
            props.showDeleteBtn && !props.probeifier.isLoading ?
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
        title={props.probeifier.isLoading ? "" : (t("Configuring") + " " + props.channelName.slice(5))}
        onClose={props.probeifier.isLoading ? undefined : props.onClose}
        onSave={props.probeifier.isLoading ? undefined : saveFn}
    >
        {props.probeifier.isLoading ?
            <div className="btnRow modalLoad">
                <Loading text={"Probing"}/>
            </div> : <>
        <div className="btnRow">
            <div className="btnDesc">
                <span>{t("MediaSource")}</span>

                {t("MediaSourceDesc")}
            </div>

            <BoxRadioGroup<DecklinkPort>
                onSelected={(v) => {
                    setChannel(fuzzyApply(channel, DECKLINK_PORT_SETTINGS[v]));
                }}
                selectedItem={fuzzyInputMatch(channel)}
                items={
                    Object.keys(DECKLINK_PORT_SETTINGS).map((i) => {
                        const k = i as DecklinkPort;
                        const status = appCtx.getPortStatus(k);
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
                <span>{t("Variants")}</span>
                {t("VariantsDesc")}
            </div>

            <BoxBtn
                label={prettyPrintResolution(...RES_4k)}
                active={hasStreamWithResolution(channel, ...RES_4k) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_4k)}
                onClick={() => toggleStreamExistence(...RES_4k)}
            ></BoxBtn>
            <BoxBtn
                label={prettyPrintResolution(...RES_1080p)}
                active={hasStreamWithResolution(channel, ...RES_1080p) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_1080p)}
                onClick={() => toggleStreamExistence(...RES_1080p)}
            ></BoxBtn>
            <BoxBtn
                label={prettyPrintResolution(...RES_720p)}
                active={hasStreamWithResolution(channel, ...RES_720p) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_720p)}
                onClick={() => toggleStreamExistence(...RES_720p)}
            ></BoxBtn>
            <BoxBtn
                label={prettyPrintResolution(...RES_360p)}
                active={hasStreamWithResolution(channel, ...RES_360p) != null}
                disabled={!inputIsAtLeast(appCtx, channel, ...RES_360p)}
                onClick={() => toggleStreamExistence(...RES_360p)}
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
                visible={hasStreamWithResolution(channel, ...RES_360p) != null}
                onClick={() => openSettingsFor(...RES_360p)}
            >
                <Cog/>
            </BoxBtn>
        </div>
        </>}
     </Modal>;
});
