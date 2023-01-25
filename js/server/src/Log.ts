export class Logger {
    constructor(private readonly name: string) {}

    fatal(message: string): void {
        console.error(`Error: [${this.name}] ${message}`);
        throw new Error(message);
    }

    error(message: string): void {
        console.error(`Error: [${this.name}] ${message}`);
    }

    warn(message: string): void {
        console.warn(`Warning: [${this.name}] ${message}`);
    }

    info(message: string): void {
        console.info(`Info: [${this.name}] ${message}`);
    }

    debug(message: string): void {
        console.debug(`Debug: [${this.name}] ${message}`);
    }
}
