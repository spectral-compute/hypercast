import './Modal.sass';
import {ReactComponent as Cross} from "../assets/icons/x-circle.svg";
import {ReactComponent as Save} from "../assets/icons/save.svg";
import {useTranslation} from "react-i18next";

export interface ModalProps {
    title: string;
    children: any;

    endBtn?: any;

    onClose?: () => void;
    onSave?: () => void;
}

function Modal(props: ModalProps) {
    const {t} = useTranslation();

    return <div className="modalBg">
        <div className="modal">
            <div className="modalHead">
                <h2>{props.title}</h2>
                {props.endBtn ? props.endBtn : null}
            </div>
            {props.children}

            <div className="modalBtnRow">
                {props.onClose ?
                <div className="modalBtn left" onClick={props.onClose}>
                    <Cross/>{t("Cancel")}
                </div> : null}
                {props.onSave ?
                <div className="modalBtn right" onClick={props.onSave}>
                    <Save/>{t("Save")}
                </div> : null}
            </div>
        </div>
     </div>;
}

export default Modal;
