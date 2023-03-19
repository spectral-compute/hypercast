import React from 'react';
import ReactDOM from 'react-dom/client';
import './index.css';
import App from './App';
import reportWebVitals from './reportWebVitals';
import {AppCtx} from "./AppCtx";
import {initialiseTranslationLibrary} from "../i18n/i18n";

// Global-ish state.
const TheAppContext = new AppCtx();
export const AppContext = React.createContext(TheAppContext);

async function bootup() {
    // It sometimes does a fetch :D
    await initialiseTranslationLibrary();
}

void bootup().then(() => {
    const root = ReactDOM.createRoot(
        document.getElementById('root') as HTMLElement
    );
    root.render(
        <React.StrictMode>
            <AppContext.Provider value={TheAppContext}>
                <App/>
            </AppContext.Provider>
        </React.StrictMode>
    );

    // If you want to start measuring performance in your app, pass a function
    // to log results (for example: reportWebVitals(console.log))
    // or send to an analytics endpoint. Learn more: https://bit.ly/CRA-vitals
    reportWebVitals();
});
