/**
 * Extend EventTarget with `on`, `off`, and `once` (nodejs-style), because they're handy.
 *
 * Of course, `addEventListener` and `removeEventListener` are provided by EventTarget.
 */
export class EventDispatcher<
    KS extends (keyof EventMap) & string,
    EventMap extends {[K in KS]: Event}
> extends EventTarget {
    constructor() {
        super();
    }

    // Constrain the key type to T.
    override addEventListener<K extends (keyof EventMap) & string>(
        type: K,
        callback: EventListenerOrEventListenerObject,
        options?: AddEventListenerOptions
    ): void {
        super.addEventListener(type as string, callback as EventListener, options);
    }

    on<K extends (keyof EventMap) & string>(evt: K, fn: (e: EventMap[K]) => void) {
        this.addEventListener(evt, fn as any);
    }
    off<K extends (keyof EventMap) & string>(evt: K, fn: (e: EventMap[K]) => void) {
        this.removeEventListener(evt as string, fn as EventListener);
    }

    once<K extends (keyof EventMap) & string>(evt: K, fn: (e: EventMap[K]) => void) {
        this.addEventListener(evt, fn as any, {once: true});
    }
}
