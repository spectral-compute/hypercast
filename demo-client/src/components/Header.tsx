import "./Header.scss";


export default function Header() {
    return <header>
        <div className="content-width">
            <div className="title">Rapid Internet Streaming Engine (RISE)</div>
            <div className="credits">
                <span>by</span>
                <a
                    href="https://spectralcompute.co.uk/"
                    target="_blank"
                    rel="author noreferrer"
                >
                    <img src="/img/spectral-compute-logo.svg" alt="Spectral Compute" />
                </a>
            </div>
        </div>
    </header>;
}
