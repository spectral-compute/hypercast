/* Fetches a url as a string (promise). */
export function fetchString(url: string): Promise<string> {
    return fetch(url).then((response: Response): Promise<string> => {
        return response.text();
    });
}

/* Gives us a clock that's synchronized to the given URL. This gets us to within a second. */
export class SynchronizedClock {
    constructor(referenceTime: string) {
        this.offset = Date.now().valueOf() - new Date(referenceTime).valueOf();
        console.log('Clock offset: ' + this.offset + ' ms');
    }

    now(): number {
        return Date.now().valueOf() - this.offset;
    }

    private offset: number;
};
