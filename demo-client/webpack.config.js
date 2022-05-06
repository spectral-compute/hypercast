const CopyWebpackPlugin = require('copy-webpack-plugin');

var config = {
    entry: "./app.ts",
    output: {
        filename: "app.js",
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
    plugins: [
        new CopyWebpackPlugin({
            patterns: [
                "./index.html"
            ]
        })
    ],
    resolve: {
        extensions: [".ts"]
    }
};

module.exports = (env, argv) => {
    if (argv.mode === "development") {
        config.devtool = "source-map";
    }
    return config;
};
