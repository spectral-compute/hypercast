declare global {
    namespace process {
        let env: {
            INFO_URL: string | null;
            NODE_ENV: string;
            SECONDARY_AUDIO: boolean;
            SECONDARY_VIDEO: boolean;
        };
    }
}

export {};
