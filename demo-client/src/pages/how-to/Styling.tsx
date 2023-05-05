import {Link} from "react-router-dom";

import StyledCode from "../../components/StyledCode";


const MDN = "https://developer.mozilla.org/en-US/docs/Web/Guide/Audio_and_video_delivery/Video_player_styling_basics";

export default function StylingHowTo() {
    // TODO: className prop for React
    // TODO: Verify claims

    const structure = `\
<!--
    The container of the player.
    If the player is used with a React library,
      this is the element that is returned by the component.
    If the player is used in a vanilla JavaScript app,
      this is the element that is passed to the createPlayer function.
-->
<div class="video-container">
    <figure class="video-figure">
        <video class="video"></video>
        <div class="video-controls">
            <!--
                This is the play/pause button.1
                It will also have a "start" or "stop" class set depending
                  on what variant of the button is actually shown.
            -->
            <button class="start-stop ...">...</button>
            <select class="quality">...</select>
            <button class="fullscreen">...</button>
        </div>
    </figure>
</div>
`;

    return <>
        <h2>Applying custom styles to the Player</h2>
        <p>
            You are free to style the player with custom CSS.
            When the player is used in a React app, a desided <code>className</code> can be passed to the component.
            When the player is used in a vanilla JavaScript app, the <code>class</code> can be specified on
            the container element that is passed to the <code>createPlayer</code> function.
        </p>
        <p>
            The player provides its own set of UI elements that can be styled with CSS.
            However, turning off the control elements or using native controls is also possible with
            the options passed to the <code>createPlayer</code> function.
            See <a href={MDN}>Video player styling basics (MDN)</a> for extra information.
        </p>
        <p>
            The player with custom controls has the following structure:
        </p>
        <StyledCode language="xml" code={structure}/>
        <p>
            See an example of a styled player in the <Link to="/demo/styling">Styling Demo</Link>.
        </p>
    </>;
}
