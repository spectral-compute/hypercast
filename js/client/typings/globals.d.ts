declare global {
    namespace process {
        let env: {
            NODE_ENV: string
        };
    }
}

export {}
