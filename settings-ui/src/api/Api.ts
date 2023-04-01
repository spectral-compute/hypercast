import {BaseResponse, SetStreamingStateResponse} from "./Types";
import {assertType} from "@ckitching/typescript-is";

// We assume that we're being served by the server, so a relative url can be used to reach the API.
// It's also useful to run the settings ui in the webpack devserver, so in dev builds we allow a URL
// parameter to be used to specify where the server is.
let API_BASE = "../api";
const urlParams = new URLSearchParams(window.location.search);
if (process.env.NODE_ENV === "development") {
    if (urlParams.has('api')) {
        API_BASE = urlParams.get("api")!;
    }
}

// An object so the API can be stateful. It's not 100% clear if it's actually going to be so, but I CBA to refactor
// it if it ever becomes so :D
export class Api {
    async makeRequest(method: string, route: string, params: any = {}) {
        const paramsObj = new URLSearchParams(params);
        if (method === "GET") {
            // Send params as GET params.
            const paramStr = paramsObj.toString();
            if (paramStr !== "") {
                route += "?" + paramStr;
            }
        }

        const rsp = await fetch(API_BASE + route, {
            method,

            // In POST mode, send params as form-data.
            ...(method === "POST" ? {body: paramsObj} : {})
        });

        if (!rsp.ok) {
            throw new Error("Failed to communicate with API");
        }

        const reply: BaseResponse = JSON.parse(await rsp.text());
        if (reply.error) {
            throw new Error("Error from API: " + reply.error);
        }

        return reply;
    }

    async setStreamingState(active: boolean) {
        const reply = await this.makeRequest("POST", "/set_state", {active});
        assertType<SetStreamingStateResponse>(reply);
        return reply;
    }
}
