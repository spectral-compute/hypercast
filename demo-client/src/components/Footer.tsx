import "./Footer.scss";

function SpecA({location, children}: {location: string, children: React.ReactNode}) {
    return <a
        href={"https://spectralcompute.co.uk/" + location}
        target="_blank"
        rel="noreferrer"
    >{children}</a>;
}

export default function Footer() {
    return <footer>
        <div className="content-width footer">
            <div>
                <span className="title">Product</span>
                <ul>
                    <li><SpecA location="rise">Information</SpecA></li>
                </ul>
            </div>
            <div>
                <span className="title">Company</span>
                <ul>
                    <li><SpecA location="company">About</SpecA></li>
                    <li><SpecA location="#contact">Contact</SpecA></li>
                </ul>
            </div>
            <div className="credits">
                <SpecA location="">
                    <img src="/img/spectral-compute-logo.svg" alt="Spectral Compute" />
                </SpecA>
            </div>
        </div>
    </footer>;
}
