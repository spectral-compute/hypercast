import {Link} from "react-router-dom";

import "./Header.scss";


export default function Header() {
    return <header>
        <div className="content-width">
            <Link to="/">
                <div className="title">Ultra Low Latency Video Streamer</div>
            </Link>
            <p>
                by <a href="https://spectralcompute.co.uk/" target="_blank">Spectral Compute</a>
            </p>
        </div>
    </header>;
}
