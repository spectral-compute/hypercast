import SyntaxHighlighter from "react-syntax-highlighter";
import {a11yDark} from "react-syntax-highlighter/dist/esm/styles/hljs";


export default function StyledTypescript({code}: {code: string}) {
    return <SyntaxHighlighter language="typescript" style={a11yDark}>
        {code}
    </SyntaxHighlighter>;
}
