import {ReactComponent as Logo} from "./assets/logo.svg";
import "./LogoCard.sass";

function LogoCard() {
    return <div className="logoCardBody">
        <Logo width="3em"></Logo>
        <span>RISE</span>
    </div>;
}

export default LogoCard;
