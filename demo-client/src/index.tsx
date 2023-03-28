import React from "react";
import ReactDOM from "react-dom/client";

import Body from "./components/Body";
import Footer from "./components/Footer";
import Header from "./components/Header";

import "./index.scss";
import TestStream from "./pages/TestStream";

ReactDOM.createRoot(document.getElementById("root")!).render(
    <React.StrictMode>
        <Header/>
        <Body>
            <TestStream />
        </Body>
        <Footer/>
    </React.StrictMode>
);
