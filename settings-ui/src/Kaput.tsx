import {ReactComponent as Sadface} from "./assets/icons/frown.svg";
import "./Kaput.sass";
import {useTranslation} from "react-i18next";

export interface KaputProps {
    message?: string;
}

export default (props: KaputProps) => {
    const {t} = useTranslation();

    return <div className="kaput">
        <Sadface width="5em" height="5em"/>
        {props.message ?? t("ShitsFuckedYo")}
    </div>;
};
