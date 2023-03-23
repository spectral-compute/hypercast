import './Modal.sass';

export interface ModalProps {
    title: string;
    children: any;
}

function Modal(props: ModalProps) {
    return <div className="modalBg">
        <div className="modal">
            <h2>{props.title}</h2>
            {props.children}
        </div>
     </div>;
}

export default Modal;
