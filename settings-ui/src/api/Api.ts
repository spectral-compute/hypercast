import {BaseResponse} from "./Types";
import { StreamingConfig } from "./Config";
import { MediaSourceInfo } from "./Hardware";

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
        if (method === "GET") {
            console.log(params);
            const paramsObj = new URLSearchParams(params);
            // Send params as GET params.
            const paramStr = paramsObj.toString();
            if (paramStr !== "") {
                route += "?" + paramStr;
            }
        }

        const rsp = await fetch(`${API_BASE}/${route}`, {
            method,

            // In POST mode, send params as form-data.
            ...((method === "POST" || method == "PUT") ? {body: JSON.stringify(params)} : {})
        });

        if (!rsp.ok) {
            throw new Error("Failed to communicate with API");
        }

        let reply!: BaseResponse;
        try {
            reply = JSON.parse(await rsp.text());
        } catch (e) {
            throw new Error("Error calling API: " + e);
        }

        if (reply.error) {
            throw new Error("Error from API: " + reply.error);
        }

        return reply as any;
    }

    async setStreamingState(active: boolean) {
        const reply = await this.makeRequest("POST", "set_state", {active});
        // assertType<SetStreamingStateResponse>(reply);
        return reply;
    }

    async applyConfig(newCfg: StreamingConfig): Promise<StreamingConfig> {
        await this.makeRequest("PUT", "config", newCfg);
        return await this.loadConfig();
    }

    async probe(sources: string[]): Promise<MediaSourceInfo[]> {
        return await this.makeRequest("POST", "probe", sources.map(x => {
            return {url: x};
        }));
    }

    async loadConfig(): Promise<StreamingConfig> {
        const reply = await this.makeRequest("GET", "config");
        console.log(reply);
        // assertType<SetStreamingStateResponse>(reply);
        return reply;
    }
}
