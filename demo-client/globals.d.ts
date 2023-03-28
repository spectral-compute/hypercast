declare global {
    namespace process {
        let env: {
            REACT_APP_INFO_URL: string | undefined;
            NODE_ENV: string;
            SECONDARY_AUDIO: boolean;
            SECONDARY_VIDEO: boolean;
        };
    }
}

export {};
