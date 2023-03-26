// This is a hook to allow monkeypatching create-react-app's blackbox webpack config of doom.

const ForkTsCheckerWebpackPlugin = require("react-dev-utils/ForkTsCheckerWebpackPlugin");

function matchesAny(regexes, string) {
    if (!regexes) {
        return false; // Wut.
    }

    // Maybe it's just a lone regex..
    if (regexes.length === undefined) {
        return regexes.test(string);
    }

    for (let r of regexes) {
        if (r.test(string)) {
            return true;
        }
    }

    return false;
}

module.exports = function override(config, env) {
    // let rules = config.module.rules[1].oneOf;
    // config.module.rules[0].test = /\.(css)$/;
    const srcmapLoader = config.module.rules[0].loader;
    const numRules = config.module.rules.length;
    const rules = config.module.rules[numRules - 1].oneOf;

    // - Switch off typescript transpilation in babel.
    // - Add ts-loader to do typescript compilation before babel runs. This will respect our tsconfig.json.
    // for (let i = 0; i < rules.length; i++) {
    //     const r = rules[i];
    //     if (matchesAny(r.test, ".mjs") && matchesAny(r.test, ".tsx")) {
    //         r.options.presets[0][1] = {
    //             runtime: 'automatic',
    //             typescript: false
    //         };
    //
    //         rules[i] = {
    //             test: /\.(js|mjs|jsx|ts|tsx)$/,
    //             include: r.include,
    //             use: [{
    //                 // ... Then babel-ify it.
    //                 loader: r.loader,
    //                 options: r.options,
    //             }, {
    //                 loader: srcmapLoader
    //             }, {
    //                 // Compile TS first.
    //                 loader: "ts-loader",
    //                 options: {
    //                     "projectReferences": true
    //                 }
    //             }]
    //         }
    //
    //         break;
    //     }
    // }

    // Delete the async typecheck plugin because we now do it synchronously through ts-loader.
    // It would make sense to use ts-loader in transpileOnly mode and keep this one, but that doesn't.
    // work since transpileOnly also disables transformers :D.
    // config.plugins.splice(config.plugins.findIndex((p => p instanceof ForkTsCheckerWebpackPlugin)), 1);

    // Polyfill for nodejs `util`, since ts-is uses it. Can perhaps be removed...?
    if (!config.resolve) {
        config.resolve = {};
    }
    if (!config.resolve.fallback) {
        config.resolve.fallback = {};
    }
    config.resolve.fallback["util"] = require.resolve("util/");

    return config;
}
