import SyntaxHighlighter from "react-syntax-highlighter";
import {a11yDark} from "react-syntax-highlighter/dist/esm/styles/hljs";


export default function StyledCode({code, language = "typescript"}: {code: string, language?: string}) {
    return <SyntaxHighlighter language={language} style={a11yDark}>
        {code}
    </SyntaxHighlighter>;
}
