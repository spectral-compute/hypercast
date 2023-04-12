const TypescriptDeclarationPlugin = require('typescript-declaration-webpack-plugin');
const path = require("path");

var config = {
    target: "browserslist",
    entry: "./src/index.ts",
    output: {
        filename: "live-video-streamer-client.js",
        clean: true,
        globalObject: 'this',
        library: {
            name: 'live-video-streamer-client',
            type: 'umd',
        },
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
                            ],
                            plugins: [
                                "@babel/plugin-transform-runtime"
                            ]
                        }
                    },
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
        new TypescriptDeclarationPlugin({
            out: "live-video-streamer-client.d.ts",
            removeComments: false
        })
    ],
    resolve: {
        alias: {
            'live-video-streamer-common': path.resolve(__dirname, '../common/src'),
        },
        fallback: {
            "util": require.resolve("util/")
        },
        extensions: [".ts", ".js"]
    }
};

module.exports = (env, argv) => {
    console.log(argv.mode);

    if (argv.mode === "development") {
        config.devtool = "source-map";
    }
    return config;
};
