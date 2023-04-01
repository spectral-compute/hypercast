const webpack = require('webpack');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const path = require("path");

module.exports = (env, argv) => {
    const defEnv = (name, def) => {
        return JSON.stringify((env[name] === undefined) ? def : env[name]);
    };

    config = {
        entry: "./app.ts",
        mode: (env["NODE_ENV"] === "development" ? "development" : "production"),
        devtool: 'source-map',
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
                        {
                            loader: "ts-loader",
                            options: {
                                projectReferences: true
                            }
                        }
                    ]
                }
            ]
        },
        plugins: [
            new CopyWebpackPlugin({
                patterns: [
                    "./index.html"
                ]
            }),
            new webpack.DefinePlugin({
                "process.env.INFO_URL": defEnv("INFO_URL", null),
                "process.env.SECONDARY_VIDEO": defEnv("SECONDARY_VIDEO", false),
                "process.env.NODE_DEBUG": defEnv("NODE_DEBUG", undefined)
            })
        ],
        resolve: {
            alias: {
                'live-video-streamer-client': path.resolve(__dirname, '../client/src'),
            },
            extensions: [".ts", ".js"]
        }
    };

    if (argv.mode === "development") {
        config.devtool = "source-map";
    }

    return config;
};
