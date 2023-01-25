import {argv} from "process";
import {Config, loadConfig} from "./Config/Config";
import {Logger} from "./Log";
import {VideoServer} from "./VideoServer";

const log = new Logger("Main");

const args = argv.slice(2);
if (args.length !== 1) {
    log.fatal(`Bad command line arguments. Usage: ${argv[0]!} ${argv[1]!} Config`);
}

const config: Config = loadConfig(args[0]!);
new VideoServer(config);
