import {ReactNode} from "react";

import Header from "../components/Header";
import Body from "../components/Body";
import Footer from "../components/Footer";
import Path from "../components/nav/Path";

export default function ({children}: {children?: ReactNode}) {
    return <>
        <Header/>
        <Body>
            <Path/>
            {children}
        </Body>
        <Footer/>
    </>;
}
