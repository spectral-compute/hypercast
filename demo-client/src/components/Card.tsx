import "./Card.scss";


export function CardContainer({children}: {children: React.ReactNode}) {
    return <div className="card-container">
        {children}
    </div>;
}


export default function Card({children}: {children: React.ReactNode}) {
    return <div className="card">
        {children}
    </div>;
}
