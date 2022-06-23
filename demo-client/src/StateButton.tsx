import React from "react";

/**
 * A button that switches between states when clicked.
 */
export function StateButton(args: {
    /**
     * List of states to cycle between.
     */
    states: {
        /**
         * The button text when in this state.
         */
        name: string,

        /**
         * A function to call when entering this state.
         *
         * It's only called when the button is clicked, not as a result of being the default state.
         */
        fn: () => void
    }[],

    /**
     * The initial state.
     *
     * Default: 0.
     */
    initialState?: number
}): React.ReactElement<HTMLDivElement> {
    // Defaults.
    const props = {
        initialState: 0,
        ...args
    };

    const [state, setState] = React.useState<number>(props.initialState);
    return (
        <div id="mute" className="button" onClick={
            (): void => {
                props.states[state]!.fn();
                setState((state + 1) % props.states.length);
            }
        }>{props.states[state]!.name}</div>
    );
}
