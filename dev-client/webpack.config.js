const webpack = require('webpack');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const path = require("path");

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
        })
    ],
    resolve: {
        alias: {
            'live-video-streamer-client': path.resolve(__dirname, '../client/src'),
        },
        extensions: [".ts", ".js"]
    }
};

module.exports = (env, argv) => {
    if (argv.mode === "development") {
        config.devtool = "source-map";
    }

    const defEnv = (name, def) => {
        return JSON.stringify((env[name] === undefined) ? def : env[name]);
    };
    config.plugins.push(new webpack.DefinePlugin({
        "process.env.INFO_URL": defEnv("INFO_URL", null)
    }));
    config.plugins.push(new webpack.DefinePlugin({
        "process.env.SECONDARY_VIDEO": defEnv("SECONDARY_VIDEO", false)
    }));

    return config;
};
