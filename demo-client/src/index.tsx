import React from "react";
import ReactDOM from "react-dom/client";
import {RouterProvider, createBrowserRouter} from "react-router-dom";

import Body from "./components/Body";
import Footer from "./components/Footer";
import Header from "./components/Header";

// Pages
import Index from "./pages/Index";
import Demo from "./pages/Demo";
import HowTo from "./pages/HowTo";
import NotFound from "./pages/NotFound";

// Subpages for Demo

import DemoIndex from "./pages/demo/Index";
import DemoLatency from "./pages/demo/Latency";
import DemoStyling from "./pages/demo/Styling";

// Subpages for HowTo

import HowToIndex from "./pages/how-to/Index";
import HowToReact from "./pages/how-to/React";
import HowToStyling from "./pages/how-to/Styling";
import HowToVanillaJS from "./pages/how-to/VanillaJS";


import "./index.scss";


const router = createBrowserRouter([
    {
        path: "/",
        element: <Index />,
    },
    {
        path: "/demo",
        element: <Demo />,
        // Demo subpages
        children: [
            {
                path: "/demo/",
                element: <DemoIndex />,
            },
            {
                path: "/demo/latency",
                element: <DemoLatency />,
            },
            {
                path: "/demo/styling",
                element: <DemoStyling />,
            },
        ],
    },
    {
        path: "/how-to",
        element: <HowTo />,
        // HowTo subpages
        children: [
            {
                path: "/how-to/",
                element: <HowToIndex />,
            },
            {
                path: "/how-to/react",
                element: <HowToReact />,
            },
            {
                path: "/how-to/styling",
                element: <HowToStyling />,
            },
            {
                path: "/how-to/vanilla-js",
                element: <HowToVanillaJS />,
            },
        ],
    },
    // 404
    {
        path: "*",
        element: <NotFound />,
    },
]);


ReactDOM.createRoot(document.getElementById("root")!).render(
    <React.StrictMode>
        <Header/>
        <Body>
            <RouterProvider router={router}/>
        </Body>
        <Footer/>
    </React.StrictMode>
);
