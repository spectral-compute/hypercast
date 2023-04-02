import {Link} from "react-router-dom";
import Card, {CardContainer} from "../components/Card";
import PageTemplate from "./PageTemplate";

export default function () {
    return <PageTemplate>
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
    </PageTemplate>;
}