import {Outlet} from "react-router-dom";
import PageTemplate from "./PageTemplate";

export default function () {
    return <PageTemplate>
        <h1>Player demos</h1>
        <Outlet/>
    </PageTemplate>;
}
