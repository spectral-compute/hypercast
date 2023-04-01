import {Outlet} from "react-router-dom";
import Header from "../components/Header";
import Body from "../components/Body";
import Footer from "../components/Footer";

export default function () {
    return <>
        <Header/>
        <Body>
            <h1>Learn how to use the player</h1>
            <Outlet/>
        </Body>
        <Footer/>
    </>;
}
