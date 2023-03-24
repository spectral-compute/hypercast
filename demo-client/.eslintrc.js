module.exports = {
    ...require("../eslintrc-template")
};
module.exports.extends.push("react-app");

function disable(rule) {
    module.exports.rules[rule] = ["off"];
}

// Allow not specifying return types for functions.
disable("@typescript-eslint/explicit-function-return-type");
disable("@typescript-eslint/explicit-module-boundary-types");
