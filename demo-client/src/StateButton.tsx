import React from "react";

// One of possible states of a StateButton
interface StateButtonState {
    /**
     * The button text when in this state.
     */
    name: string,

    /**
     * A function to call when entering this state.
     *
     * It's only called when the button is clicked, not as a result of being the default state.
     */
    fn: () => void,
}

export interface StateButtonProps {
    /**
     * List of states to cycle between.
     */
    states: StateButtonState[],

    /**
     * The initial state.
     *
     * Default: 0.
     */
    initialState?: number
}

/**
 * A button that switches between states when clicked.
 */
export default function StateButton({
    states,
    initialState = 0
}: StateButtonProps): React.ReactElement<HTMLDivElement> {
    const [state, setState] = React.useState<number>(initialState);
    return (
        <div id="mute" className="button" onClick={
            (): void => {
                states[state]!.fn();
                setState((state + 1) % states.length);
            }
        }>{states[state]!.name}</div>
    );
}
