var config = {
    target: "node",
    entry: "./src/Main.ts",
    output: {
        filename: "live-video-streamer-server.js",
        clean: true
    },
    module: {
        rules: [
            {
                test: /\.ts$/,
                use: [
                    "babel-loader",
                    "ts-loader"
                ]
            }
        ]
    },
    resolve: {
        extensions: [".ts", ".js"]
    }
};

module.exports = (env, argv) => {
    if (argv.mode === "development") {
        config.devtool = "source-map";
    }
    return config;
};
