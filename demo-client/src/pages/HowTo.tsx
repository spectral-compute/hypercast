import {Outlet} from "react-router-dom";
import PageTemplate from "./PageTemplate";

export default function () {
    return <PageTemplate>
        <h1>Learn how to use the player</h1>
        <Outlet/>
    </PageTemplate>;
}
