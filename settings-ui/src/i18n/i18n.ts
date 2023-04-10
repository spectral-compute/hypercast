import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import translationEN from "./translation/en.json";
import translationNL from "./translation/nl.json";
import {setDifference, setIntersection} from "live-video-streamer-common";

export const CURRENT_LANGUAGE_LS_KEY = "i18nextLng"; // Localstorage key to change.

const resources = {
  en: {
    translation: translationEN
  },
  nl: {
    translation: translationNL
  }
} as const;

export async function initialiseTranslationLibrary() {
    if (process.env["NODE_ENV"] === 'development') {
        // Compare keysets of translation files in development, to avoid mistakes.
        const englishKeys = new Set(Object.keys(resources.en.translation));
        for (const [k, v] of Object.entries(resources)) {
            if (k === "en") {
                continue; // Skip self-comparison.
            }

            const keysHere = new Set(Object.keys(v.translation));
            const intersection = setIntersection(englishKeys, keysHere);

            if (intersection.size != englishKeys.size || intersection.size != keysHere.size) {
                console.log("Localisation keyset mismatch!");
                console.log("Missing (in en but not in " + k + "):");
                console.log(setDifference(englishKeys, intersection));
                console.log("Extra:");
                console.log(setDifference(keysHere, intersection));
                throw new Error("Localisation keyset mismatch. Try again with less clumsiness ;)");
            }
        }
    }

    await i18n
        .use(initReactI18next)
        .init({
            resources,
            lng: "nl", // Plausibly wanna set this as a build flag or such.
            supportedLngs: ['en', 'nl'],
            fallbackLng: "en",

            keySeparator: false,
            interpolation: {
                skipOnVariables: false,
                escapeValue: false
            }
        });

    // ISO8601 dates, so people don't yell at us :D
    i18n.services.formatter!.add('iso8601_date', (value, _lng, _options) => {
        const s = value.toISOString();
        return s.slice(0, s.indexOf("T"));
    });
}
