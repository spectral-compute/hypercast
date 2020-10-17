import '../common/bootup';

import {VideoServer} from "./VideoServer";

const server = new VideoServer();
void server.start();
