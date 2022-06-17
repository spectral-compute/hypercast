const webpack = require('webpack');
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

    const defEnv = (name, def) => {
        return JSON.stringify((env[name] === undefined) ? def : env[name]);
    };
    config.plugins.push(new webpack.DefinePlugin({
        "process.env.INFO_URL": defEnv("INFO_URL", "http://localhost:8080/live/info.json")
    }));
    config.plugins.push(new webpack.DefinePlugin({
        "process.env.SECONDARY_AUDIO": defEnv("SECONDARY_AUDIO", false)
    }));
    config.plugins.push(new webpack.DefinePlugin({
        "process.env.SECONDARY_VIDEO": defEnv("SECONDARY_VIDEO", false)
    }));

    return config;
};
