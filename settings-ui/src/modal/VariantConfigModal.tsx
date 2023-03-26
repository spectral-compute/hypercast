import './ChannelConfigModal.sass';
import Modal from "./Modal";
import BoxBtn from "../BoxBtn";

export interface ChannelConfigModalProps {
    onClose: () => void;
}

function ChannelConfigModal(props: ChannelConfigModalProps) {
    return <Modal
        title="Configuring Channel 1"
        onClose={props.onClose}
        onSave={() => {}}
    >
        <hr/>

        <div className="btnRow">
            <div className="btnDesc">
                <span>Resolution</span>
            </div>
            <BoxBtn label="4k"></BoxBtn>
            <BoxBtn label="1080p"></BoxBtn>
            <BoxBtn label="720p"></BoxBtn>
            <BoxBtn label="480p"></BoxBtn>
        </div>


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
                <span>Target Latency</span>

                The target amount of time between an event being seen by the camera, and it reaching viewers.

                Lower latency settings may use a little more bandwidth than higher-latency ones.
            </div>
            <BoxBtn label="1s"></BoxBtn>
            <BoxBtn label="2s"></BoxBtn>
            <BoxBtn label="3s"></BoxBtn>
            <BoxBtn label="5s"></BoxBtn>
        </div>

    </Modal>;
}

export default ChannelConfigModal;
