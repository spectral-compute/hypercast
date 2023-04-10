import './ChannelConfigModal.sass';
import {AudioVariant, FrameRateSpecial, StreamVariantConfig, VideoVariant} from "../api/Config";
import BoxRadioGroup from "../components/BoxRadioGroup";
import Modal from "./Modal";
import {
    FUZZY_AUDIO_QUALITY_SETTINGS,
    FUZZY_VIDEO_QUALITY_SETTINGS, FuzzyAudioQuality, fuzzyAudioQualityMatch,
    FuzzyVideoQuality,
    fuzzyVideoQualityMatch
} from "../Constants";
import {fuzzyApply} from "../Fuzzify";
import {useState} from "react";
import {useTranslation} from "react-i18next";

export interface VariantConfigModalProps {
    onSave: (cfg: StreamVariantConfig) => void;
    onCancel: () => void;

    title: string;
    cfg: StreamVariantConfig
}

export default (props: VariantConfigModalProps) => {
    // Local copy of the config is used during editing. On cancel this is discarded, but on
    // save it's propagated to the parent (and incorporated into the in-progress cfg object).
    const [video, setVideo] = useState<VideoVariant>(props.cfg.video);
    const [audio, setAudio] = useState<AudioVariant>(props.cfg.audio);
    const [stream, setStream] = useState<StreamVariantConfig>(props.cfg);
    const {t} = useTranslation();

    function doSave() {
        const result = {...stream};
        result.video = {...video};
        result.audio = {...audio};
        props.onSave(result);
    }

    return <Modal
        title={t("Configuring") + " " + props.title}
        onClose={props.onCancel}
        onSave={() => doSave()}
    >
        <div className="btnRow">
            <div className="btnDesc">
                <span>{t("ImageQuality")}</span>

                {t("ImageQualityDesc")}
            </div>

            <BoxRadioGroup<FuzzyVideoQuality>
                onSelected={(v) => {
                    setVideo(fuzzyApply(video, FUZZY_VIDEO_QUALITY_SETTINGS[v]));
                }}
                selectedItem={fuzzyVideoQualityMatch(video)}
                items={[{
                    value: "LOW",
                    label: t("Low")!
                }, {
                    value: "MEDIUM",
                    label: t("Medium")!
                }, {
                    value: "HIGH",
                    label: t("High")!
                }, {
                    value: "VERY_HIGH",
                    label: t("Highest")!
                }]}
            ></BoxRadioGroup>
        </div>

        <hr/>

        <div className="btnRow">
            <div className="btnDesc">
                <span>{t("AudioQuality")}</span>
            </div>

            <BoxRadioGroup<FuzzyAudioQuality>
                onSelected={(v) => {
                    setAudio(fuzzyApply(audio, FUZZY_AUDIO_QUALITY_SETTINGS[v]));
                }}
                selectedItem={fuzzyAudioQualityMatch(audio)}
                items={[{
                    value: "LOW",
                    label: t("Low")!
                }, {
                    value: "MEDIUM",
                    label: t("Medium")!
                }, {
                    value: "HIGH",
                    label: t("High")!
                }]}
            ></BoxRadioGroup>
        </div>

        <hr/>

        <div className="btnRow">
            <div className="btnDesc">
                <span>{t("FrameRate")}</span>
                {t("FrameRateDesc")}
            </div>

            <BoxRadioGroup<FrameRateSpecial>
                selectedItem={video.frameRate as FrameRateSpecial}
                onSelected={(v) => setVideo({...video, frameRate: v})}
                items={[{
                    value: undefined,
                    label: t("Native")!
                }, {
                    value: "half",
                    label: t("Half")!
                }]}
            ></BoxRadioGroup>
        </div>

        <hr/>

        <div className="btnRow">
            <div className="btnDesc">
                <span>{t("TargetLatency")}</span>
                {t("TargetLatencyDesc")}
            </div>
            <BoxRadioGroup<number>
                onSelected={(v) => setStream({...stream, targetLatency: v})}
                selectedItem={stream.targetLatency}
                items={[{
                    value: 1000,
                    label: "1s",
                    disabled: stream.video.width < 1280 || stream.video.height < 720
                }, {
                    value: 2000,
                    label: "2s"
                }, {
                    value: 3000,
                    label: "3s"
                }, {
                    value: 5000,
                    label: "5s"
                }]}
            ></BoxRadioGroup>
        </div>
    </Modal>;
};
