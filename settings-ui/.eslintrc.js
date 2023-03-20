const path = require("path");

module.exports = {
    env: {
        "node": false,
    },
    parser: '@typescript-eslint/parser',
    reportUnusedDisableDirectives: true,
    ignorePatterns: [],

    extends: [
        // Enable the most aggressive presets that exist.
        "eslint:recommended",
        "plugin:@typescript-eslint/eslint-recommended",
        "plugin:@typescript-eslint/recommended",
        "plugin:@typescript-eslint/recommended-requiring-type-checking",
        "plugin:react/recommended",
        "plugin:react/jsx-runtime"
    ],
    plugins: [
        "react",
        "react-i18n",
        "i18n-json"
    ],

    parserOptions:  {
        ecmaVersion: 2018,

        // Allows use of imports
        sourceType: 'module',

        // Needed for type-dependent linters.
        tsconfigRootDir: __dirname,
        project: ['./tsconfig.json'],
    },
    root: true,

    settings: {
        react: {
            version: "detect",
            componentWrapperFunctions: [
                // The name of any function used to wrap components, e.g. Mobx `observer` function. If this isn't set, components wrapped by these functions will be skipped.
                "observer", // `property`
                {"property": "styled"}, // `object` is optional
                {"property": "observer", "object": "Mobx"},
                {"property": "observer", "object": "<pragma>"} // sets `object` to whatever value `settings.react.pragma` is set to
            ],
        }
    },

    // And now to go up to `-Weverything -Werror`... :D
    rules: {
        "prefer-const": "off",

        "@typescript-eslint/no-use-before-define": "off",
        "@typescript-eslint/triple-slash-reference": "off",

        // False-positives on TS namespaces :(
        "no-inner-declarations": "off",

        // Typings bug in express-async causes many false positives :(.
        "@typescript-eslint/no-misused-promises": "off",

        "@typescript-eslint/consistent-type-assertions": "off",

        // Allow namespaces
        "@typescript-eslint/no-namespace": "off",

        // Generated code means we cannot have nice things.
        "@typescript-eslint/camelcase": "off",

        // Allow empty functions.
        "@typescript-eslint/no-empty-function": "off",

        // Configure as warning because it very occasionally makes nondeterministic false positives :D
        "@typescript-eslint/no-unnecessary-type-assertion": "warn",

        // Permit explicit `any`
        "@typescript-eslint/no-explicit-any": "off",

        // Allow explicit type annotations on class members and function parameters, even when they are
        // inferrable, because otherwise you end up with an irritating mixture of annotated and
        // un-annotated things depending on which ones have default initialisers.
        "@typescript-eslint/no-inferrable-types": ["warn", {
            ignoreProperties: true,
            ignoreParameters: true
        }],

        // No redundant double-null-!
        "@typescript-eslint/no-extra-non-null-assertion": ["error"],

        // Don't allow `delete` with computed operand. Use `Map` instead.
        "@typescript-eslint/no-dynamic-delete": ["error"],

        // Sadly, this forbids constructs like:
        // export type AggViewDescriptors = {[name: string]: CTEDef | AggViewDef};
        // This would make it impossible to use index types in alises like this, which is kinda problematic.
        "@typescript-eslint/consistent-type-definitions": ["off"],

        // Don't allow disabling compile-type type errors using comments (apparently that's a thing...)
        "@typescript-eslint/ban-ts-comment": ["error"],

        // Always use `[]` for array types.
        "@typescript-eslint/array-type": ["error"],

        // `[[nodiscard]]` for promises.
        "@typescript-eslint/no-floating-promises": ["error", {ignoreVoid: true}],

        // No `eval()`, or things-that-sneakily-do-`eval()`.
        "@typescript-eslint/no-implied-eval": ["error"],

        "@typescript-eslint/no-non-null-asserted-optional-chain": ["error"],

        "@typescript-eslint/no-var-requires": ["error"],
        "@typescript-eslint/no-require-imports": ["error"],

        // Async validator likes to throw strings, sadly.
        "@typescript-eslint/no-throw-literal": ["off"],

        // Ban `if (someBool === true)` and suchlike.
        "@typescript-eslint/no-unnecessary-boolean-literal-compare": ["error"],

        // No redundant qualified names.
        "@typescript-eslint/no-unnecessary-qualifier": ["error"],

        // Don't redundantly specify the default value of a tparam.
        "@typescript-eslint/no-unnecessary-type-arguments": ["error"],

        // Prefer enhanced for-loops where possible.
        "@typescript-eslint/prefer-for-of": ["error"],

        // Prefer using function type instead of an interface or object type literal with a
        // single call signature.
        "@typescript-eslint/prefer-function-type": ["error"],

        // Use typescripts various null-checking and coalescing operators instead of short-circuiting logical
        // operators.
        "@typescript-eslint/prefer-nullish-coalescing": ["error"],
        "@typescript-eslint/prefer-optional-chain": ["error"],

        // Make private fields readonly where possible.
        "@typescript-eslint/prefer-readonly": ["error"],

        // All promise-returning functions shall also be `async`
        "@typescript-eslint/promise-function-async": ["error"],

        // Array.sort() shall always have a sort-predicate, because the default one is completely insane:
        //     [1, 2, 3, 10, 20, 30].sort(); //→ [1, 10, 2, 20, 3, 30]
        "@typescript-eslint/require-array-sort-compare": ["error"],

        // Detect unifiable overloads of a function.
        "@typescript-eslint/unified-signatures": ["error"],

        // This one is handled by the typescript compiler.
        "@typescript-eslint/no-unused-vars": ["off"],

        // Consistent spaces before/after commas.
        "comma-spacing": "off", // We use the TS one instead.
        "@typescript-eslint/comma-spacing": ["warn"],

        // Default parameters must be last.
        "@typescript-eslint/default-param-last": ["error"],

        // No spaces before call `(`.
        "func-call-spacing": "off", // We use the TS one instead.
        "@typescript-eslint/func-call-spacing": ["warn"],

        // Indent pedantry.
        "indent": "off", // We use the TS one instead.
        "@typescript-eslint/indent": "off",

        // No spaces before `(` on function declarations.
        "space-before-function-paren": "off", // We use the TS one instead.
        "@typescript-eslint/space-before-function-paren": ["warn", {
            "anonymous": "never",
            "named": "never",
            "asyncArrow": "ignore",
        }],

        // Spaces before type annotations and around fat arrows
        "@typescript-eslint/type-annotation-spacing": ["error"],
        "@typescript-eslint/unbound-method": "off",

        "brace-style": "off", // We use the TS one instead.
        "@typescript-eslint/brace-style": ["error", "1tbs", {
            allowSingleLine: true
        }],

        "@typescript-eslint/require-await": "off",

        // No duplicate class members.
        "no-dupe-class-members": "off", // We use the TS one instead.
        "@typescript-eslint/no-dupe-class-members": ["error"],

        // No unnecessary semicolons.
        "no-extra-semi": "off", // We use the TS one instead.
        "@typescript-eslint/no-extra-semi": ["error"],

        // Discarded expressions with no side effects are usually a bad idea...
        "no-unused-expressions": "off", // We use the TS one instead.
        "@typescript-eslint/no-unused-expressions": ["error"],

        // Don't explicitly redeclare the empty constructor.
        // Disabled because of false positives :(
        // "no-useless-constructor": "off", // We use the TS one instead.
        // "@typescript-eslint/no-useless-constructor": "error",

        // Automatic semicolon insertion is EVIL.
        "semi": "off", // We use the TS one instead.
        "@typescript-eslint/semi": ["error"],

        "@typescript-eslint/no-non-null-assertion": ["off"],

        // Allow empty interfaces (useful sometimes as marker interfaces).
        "@typescript-eslint/no-empty-interface": ["off"],

        // Can't get it to ignore typescript string literals...
        "no-irregular-whitespace": ["off"],

        // Allow non-defined return types
        "@typescript-eslint/explicit-function-return-type": ["off"],

        "@typescript-eslint/no-unsafe-argument": ["warn"],

        // Does not play nice with react-table :(
        "react/jsx-key": ["off"],

        // Camelcase is desired, but we want to match database name_format when mirroring into a structure
        // so we keep it at warn instead of error
        "camelcase": "off",
        // "@typescript-eslint/naming-convention": ["warn", {
        //
        // }],

        // Neater comments
        "spaced-comment": ["warn", "always", {
            "line": {"exceptions": ["/"], "markers": ["/"]},
            "block": {
                "exceptions": ["*"],
                "markers": ["*"],
                "balanced": true
            }
        }],

        // General encouragement of good clean form
        "space-in-parens": ["error"],
        "space-infix-ops": ["error"],
        "space-before-blocks": ["error"],
        "no-trailing-spaces": ["error"],

        // I do what I want!
        "@typescript-eslint/interface-name-prefix": ["off"],

        "@typescript-eslint/no-unsafe-member-access": ["off"],
        "@typescript-eslint/no-unsafe-assignment": ["off"],
        "@typescript-eslint/no-unsafe-call": ["off"],
        "@typescript-eslint/no-unsafe-return": ["off"],
        "@typescript-eslint/restrict-template-expressions": ["off"],
        "@typescript-eslint/ban-types": ["off"],
        "@typescript-eslint/restrict-plus-operands": ["off"],
        "@typescript-eslint/no-unnecessary-condition": ["off"],

        // We aren't writing a library.
        "@typescript-eslint/explicit-module-boundary-types": ["off"],

        // Produces false postives on antd column specifiers.
        "react/display-name": ["off"],

        // Typescript does this better.
        "react/prop-types": ["off"],

        // React apparently _merely warns you_ about the presence of insanity like child elements in `<img>` or `<br>`.
        "react/void-dom-elements-no-children": ["error"],

        // React will forbid the dangerous `javascript:` URI in a future release, so let's get ahead of that...
        "react/jsx-no-script-url": ["error"],

        // This makes it very hard to accidentally put strings in the UI that are not wrapped in the translator function.
        // This causes the linter to reject any string literal that is in JSX and not part of a `{}`-expression. In most
        // cases this is the nudge we need to remember to put in `t()`. If a string really should be never translated,
        // then you can either add it to the list below to permanently exclude it from this rule, or do `{"foo"}` at the
        // usage site for a one-off.
        "react/jsx-no-literals": ["warn", {
            "allowedStrings": [
                ".", ":", "-", "—", "%", "/", "&#8594;", "... &#8594;",
                "&middot;", "(", ")",
                "&nbsp;"  // TODO: LEARN CSS
            ],
            "noStrings": false,
            "ignoreProps": true
        }],

        // Once our esteemed UI developers learn what interpolation is, this will save them from accidentally forgetting
        // to pass the second parameter to an interpolated string localiation.
        "react-i18n/no-missing-interpolation-keys": ["error"],

        // Translation files must actually be valid json...
        "i18n-json/valid-json": ["error"],

        // The set of keys in the translation files must be identical.
        "i18n-json/identical-keys": ["error", {
            filePath: path.resolve('src/i18n/translation/en.json'),
        }],
    },
};
