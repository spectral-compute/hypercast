import "./Body.scss";


export default function Body({children}: {children: React.ReactNode}) {
    return <main>
        <div className="content-width">
            {children}
        </div>
    </main>;
}
