import React from "react";
import ReactDOM from "react-dom/client";

import Body from "./components/Body";
import Footer from "./components/Footer";
import Header from "./components/Header";
import Player from "./Player";

import "./index.scss";


/* Variables we expect to receive from the build environment. */
declare namespace process {
    let env: {
        REACT_APP_INFO_URL: string | undefined;
    };
}

ReactDOM.createRoot(document.getElementById("root")!).render(
    <React.StrictMode>
        <Header/>
        <Body>
            <Player sourceURL={process.env.REACT_APP_INFO_URL!}/>
        </Body>
        <Footer/>
    </React.StrictMode>
);
