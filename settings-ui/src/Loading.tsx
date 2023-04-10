import {ReactComponent as Logo} from "./assets/logo.svg";
import "./Loading.sass";
import {useTranslation} from "react-i18next";

function spinner() {
    // Well, this is insane.
    let is = [];
    for (let i = 0; i < 15; i++) {
        is.push(i);
    }

    return <div className="happyContainer">{
        is.map((i) => <Logo className={"happy " + " logo" + i} width="8em" height="8em"/>)
    }</div>;
}

export default () => {
    const {t} = useTranslation();

    return <div className="kaput">
        {spinner()}
        {t("Loading") + "..."}
    </div>;
};
