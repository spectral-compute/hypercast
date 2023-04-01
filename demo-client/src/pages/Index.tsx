import {Link} from "react-router-dom";
import Card, {CardContainer} from "../components/Card";
import Header from "../components/Header";
import Body from "../components/Body";
import Footer from "../components/Footer";

export default function () {
    return <>
        <Header/>
        <Body>
            <CardContainer>
                <Link to="/demo">
                    <Card>
                        <h2>Demo</h2>
                        <p>See how the player works</p>
                    </Card>
                </Link>
                <Link to="/how-to">
                    <Card>
                        <h2>How-to</h2>
                        <p>Learn how to use the player in your project</p>
                    </Card>
                </Link>
            </CardContainer>
        </Body>
        <Footer/>
    </>;
}
