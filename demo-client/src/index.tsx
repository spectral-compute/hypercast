import React from "react";
import ReactDOM from "react-dom/client";
import "./index.css";

function App(): React.ReactElement<HTMLDivElement> {
    return (<div></div>);
}

const root = ReactDOM.createRoot(document.getElementById("root")!);
root.render(
    <React.StrictMode>
        <App />
    </React.StrictMode>
);
