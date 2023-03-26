import './ChannelConfigModal.sass';
import BoxBtn from "../BoxBtn";
import {AudioVariant, VideoVariant} from "../api/Config";

export interface VariantConfigModalProps {
    audio: AudioVariant;
    video: VideoVariant;
}

export default (_props: VariantConfigModalProps) => {
    return <>
        <div className="btnRow">
            <div className="btnDesc">
                <span>Image Quality</span>

                Higher settings will consume more CPU resources to produce a better image quality at each resolution
                selected.
            </div>
            <BoxBtn label="Low"></BoxBtn>
            <BoxBtn label="Medium"></BoxBtn>
            <BoxBtn label="High"></BoxBtn>
        </div>

        <hr/>

        <div className="btnRow">
            <div className="btnDesc">
                <span>Audio Quality</span>
            </div>

            <BoxBtn label="Low"></BoxBtn>
            <BoxBtn label="Medium"></BoxBtn>
            <BoxBtn label="High"></BoxBtn>
        </div>

        <hr/>

        <div className="btnRow">
            <div className="btnDesc">
                <span>Frame Rate</span>
                Do you want to halve the framerate compared to the source material in this stream variant?
                This can improve the experience for viewers on poor connections.
            </div>

            <BoxBtn label="Native"></BoxBtn>
            <BoxBtn label="Half"></BoxBtn>
        </div>

        <hr/>

        <div className="btnRow">
            <div className="btnDesc">
                <span>Target Latency</span>

                The target amount of time between an event being seen by the camera, and it reaching viewers.

                Lower latency settings may use a little more bandwidth than higher-latency ones.
            </div>
            <BoxBtn label="1s"></BoxBtn>
            <BoxBtn label="2s"></BoxBtn>
            <BoxBtn label="3s"></BoxBtn>
            <BoxBtn label="5s"></BoxBtn>
        </div>
    </>;
};
