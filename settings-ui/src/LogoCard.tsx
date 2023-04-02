import {ReactComponent as Logo} from "./assets/rise.svg";
import "./LogoCard.sass";

function LogoCard() {
    return <div className="logoCardBody">
        <Logo width="7em"></Logo>
    </div>;
}

export default LogoCard;
