import {BaseResponse, SetStreamingStateResponse} from "./Types";
import {assertType} from "@ckitching/typescript-is";

export const API_BASE = "http://localhost:8192/api";

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