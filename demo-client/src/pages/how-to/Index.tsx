import {Link} from "react-router-dom";

import Card, {CardContainer} from "../../components/Card";


export default function () {
    return <CardContainer>
        <Link to="/how-to/vanilla-js">
            <Card>
                <h2>VanillaJS</h2>
                <p>How to use the player from pure JS</p>
            </Card>
        </Link>
        <Link to="/how-to/react">
            <Card>
                <h2>React</h2>
                <p>How to use the player with React</p>
            </Card>
        </Link>
        <Link to="/how-to/styling">
            <Card>
                <h2>Custom Styles</h2>
                <p>How to apply custom styles to the player</p>
            </Card>
        </Link>
    </CardContainer>;
}
