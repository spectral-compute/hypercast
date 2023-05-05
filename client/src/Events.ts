/**
 * Dispatched when an error occurs.
 *
 * Your handler should call `preventDefault()` if you have mitigated the error successfully in some way, otherwise the
 * implementation will throw the error, probably crashing the player.
 */
export class PlayerErrorEvent extends Event {
    constructor(public e: Error) {
        super("error", {cancelable: true});
    }
}

/**
 * Dispatched when Player's properties change.
 *
 * This includes (but not limited to):
 * - A video starts playing:
 *   - Just when it starts;
 *   - When different quality is selected for latency reasons;
 * - List of available channels changes;
 */
export class PlayerUpdateEvent extends Event {
    constructor(public elective: boolean) {
        super("update");
    }
}


/**
 * Dispatched with the object given to the send_user_json channel API when it is called.
 */
export class PlayerObjectBroadcastEvent extends Event {
    constructor(public o: any) {
        super("broadcast_object");
    }
}

/**
 * Dispatched with the object given to the send_user_binary channel API when it is called.
 */
export class PlayerBinaryBroadcastEvent extends Event {
    constructor(public data: ArrayBuffer) {
        super("broadcast_binary");
    }
}

/**
 * Dispatched with the string given to the send_user_string channel API when it is called.
 */
export class PlayerStringBroadcastEvent extends Event {
    constructor(public s: string) {
        super("broadcast_string");
    }
}

/// Event keys used by the Player class.
export interface PlayerEventMap {
    "error": PlayerErrorEvent;
    "update": PlayerUpdateEvent;
    "broadcast_object": PlayerObjectBroadcastEvent;
    "broadcast_binary": PlayerBinaryBroadcastEvent;
    "broadcast_string": PlayerStringBroadcastEvent;
}
