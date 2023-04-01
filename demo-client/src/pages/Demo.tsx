import {Outlet} from "react-router-dom";
import Header from "../components/Header";
import Body from "../components/Body";
import Footer from "../components/Footer";

export default function () {
    return <>
        <Header/>
        <Body>
            <h1>Player demos</h1>
            <Outlet/>
        </Body>
        <Footer/>
    </>;
}
