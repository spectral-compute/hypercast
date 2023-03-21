import React from 'react';
import ReactDOM from 'react-dom/client';
import './index.css';
import App from './App';
import {AppCtx} from "./AppCtx";
import {initialiseTranslationLibrary} from "./i18n/i18n";
import {assertType} from "@ckitching/typescript-is";

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
});
