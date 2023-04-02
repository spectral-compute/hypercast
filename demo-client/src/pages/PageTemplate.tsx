import Header from "../components/Header";
import Body from "../components/Body";
import Footer from "../components/Footer";

export default function ({children}: {children?: React.ReactNode}) {
    return <>
        <Header/>
        <Body>
            {children}
        </Body>
        <Footer/>
    </>;
}
