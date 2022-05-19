const NodePolyfillPlugin = require("node-polyfill-webpack-plugin")
const TypescriptDeclarationPlugin = require('typescript-declaration-webpack-plugin');

var config = {
    target: "browserslist",
    entry: "./src/Main.ts",
    output: {
        filename: "live-video-streamer-client.js",
        libraryTarget: "umd",
        clean: true
    },
    module: {
        rules: [
            {
                test: /\.ts$/,
                use: [
                    {
                        loader: "babel-loader",
                        options: {
                            presets: [
                                "@babel/preset-env"
                            ]
                        }
                    },
                    "ts-loader"
                ]
            }
        ]
    },
    plugins: [
        new NodePolyfillPlugin(),
        new TypescriptDeclarationPlugin({
            out: "live-video-streamer-client.d.ts",
            removeComments: false
        })
    ],
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
