import "./Footer.scss";

export default function Footer() {
    return <footer>
        <div className="content-width">
            <div className="footer-block">
                <span className="title">Company</span>
                <ul>
                    <li><a
                        href="https://spectralcompute.co.uk/company"
                        target="_blank"
                        rel="noreferrer"
                    >
                        About
                    </a></li>
                    <li><a
                        href="https://spectralcompute.co.uk/#contact"
                        target="_blank"
                        rel="noreferrer"
                    >
                        Contact
                    </a></li>
                </ul>
            </div>
        </div>
    </footer>;
}
