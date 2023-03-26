import './Modal.sass';
import {ReactComponent as Cross} from "../assets/icons/x-circle.svg";
import {ReactComponent as Save} from "../assets/icons/save.svg";

export interface ModalProps {
    title: string;
    children: any;

    onClose: () => void;
    onSave: () => void;
}

function Modal(props: ModalProps) {
    return <div className="modalBg">
        <div className="modal">
            <h2>{props.title}</h2>
            {props.children}

            <div className="modalBtnRow">
                <div className="modalBtn left" onClick={props.onClose}>
                    <Cross/>Cancel
                </div>
                <div className="modalBtn right" onClick={props.onSave}>
                    <Save/>Save
                </div>
            </div>
        </div>
     </div>;
}

export default Modal;
