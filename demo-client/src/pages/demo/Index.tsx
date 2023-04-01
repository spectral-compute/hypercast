import {Link} from "react-router-dom";

import Card, {CardContainer} from "../../components/Card";

export default function () {
    return <CardContainer>
        <Link to="/demo/latency">
            <Card>
                <h2>Latency</h2>
                <p>See how low the latency is</p>
            </Card>
        </Link>
        <Link to="/demo/styling">
            <Card>
                <h2>Styling</h2>
                <p>Examples of player styling</p>
            </Card>
        </Link>
    </CardContainer>;
}
