import "./Footer.scss";

function Spec_a({location, children}: {location: string, children: React.ReactNode}) {
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
                    <li><Spec_a location="rise">Information</Spec_a></li>
                </ul>
            </div>
            <div>
                <span className="title">Company</span>
                <ul>
                    <li><Spec_a location="company">About</Spec_a></li>
                    <li><Spec_a location="#contact">Contact</Spec_a></li>
                </ul>
            </div>
            <div className="credits">
                <span>by</span>
                <Spec_a location="">
                    <img src="/img/spectral-compute-logo.svg" alt="Spectral Compute" />
                </Spec_a>
            </div>
        </div>
    </footer>;
}
