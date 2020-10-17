import {Logger} from "winston";
import {getLogger} from "../common/LoggerConfig";
import {ZmqEndpoint, ZmqTarget} from "../common/network/ZmqLayer";
import {WebServerNode} from "../common/WebServerNode";
import http from "http";

export const log: Logger = getLogger("VideoServer");


export class VideoServer extends WebServerNode {
    constructor() {
        super("VideoServer", ZmqEndpoint.VIDEO_SERVER, ZmqEndpoint.NONE, ZmqTarget.NONE);
    }

    protected buildServer(listenCallback: () => void): http.Server {

    }
}
