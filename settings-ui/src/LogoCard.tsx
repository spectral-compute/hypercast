import {ReactComponent as Logo} from "./logo.svg";
import BoxThing from "./BoxThing";
import "./LogoCard.sass";

function LogoCard() {
    return <div className="logoCard">
        <BoxThing>
            <div className="logoCardBody">
                <Logo width="2em"></Logo>
                <span>RISE</span>
            </div>
        </BoxThing>
    </div>;
}

export default LogoCard;
