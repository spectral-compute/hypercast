module.exports = {
    parser: "@typescript-eslint/parser",
    reportUnusedDisableDirectives: true,
    extends: [
        "eslint:recommended",
        "plugin:@typescript-eslint/eslint-recommended",
        "plugin:@typescript-eslint/recommended",
        "plugin:@typescript-eslint/recommended-requiring-type-checking"
    ],
    parserOptions: {
        /* Don't be limited to ancient version of ECMAScript. */
        ecmaVersion: "latest",

        /* Allows use of imports. */
        sourceType: "module",

        /* Needed for type-dependent linters. */
        tsconfigRootDir: __dirname,
        project: ["./tsconfig.json", "client/tsconfig.json", "server/tsconfig.json"],
    },
    rules: {
        /* Non-default base ESLint rules that we want. */
        "eqeqeq": ["error"],
        "no-constant-binary-expression": ["error"],
        "no-duplicate-imports": ["error"],
        "no-extra-bind": ["error"],
        "no-extra-label": ["error"],
        "no-implicit-coercion": ["error"],
        "no-implied-eval": ["error"],
        "no-label-var": ["error"],
        "no-multi-assign": ["error"],
        "no-self-compare": ["error"],
        "no-unreachable-loop": ["error"],
        "no-unused-private-class-members": ["error"],
        "no-useless-rename": ["error"],
        "no-useless-return": ["error"],
        "no-var": ["error"],
        "prefer-exponentiation-operator": ["error"],
        "prefer-numeric-literals": ["error"],
        "prefer-object-has-own": ["error"],
        "prefer-object-spread": ["error"],
        "prefer-promise-reject-errors": ["error"],
        "require-atomic-updates": ["error"],
        "unicode-bom": ["error", "never"],

        // Code style.
        "arrow-spacing": ["error"],
        "block-spacing": ["error"],
        "comma-spacing": ["error"],
        "comma-style": ["error"],
        "computed-property-spacing": ["error"],
        "curly": ["error", "all"],
        "default-case-last": ["error"],
        "dot-location": ["error"],
        "eol-last": ["error"],
        "generator-star-spacing": ["error"],
        "implicit-arrow-linebreak": ["error"],
        "key-spacing": ["error"],
        "linebreak-style": ["error"],
        "max-len": ["error", {
            code: 120,
            ignoreUrls: false
        }],
        "new-cap": ["error"],
        "new-parens": ["error"],
        "no-floating-decimal": ["error"],
        "no-lonely-if": ["error"],
        "no-multi-spaces": ["error"],
        "no-tabs": ["error"],
        "no-trailing-spaces": ["error"],
        "no-whitespace-before-property": ["error"],
        "operator-linebreak": ["error", "after"],
        "padded-blocks": ["error", "never"],
        "rest-spread-spacing": ["error"],
        "semi-spacing": ["error"],
        "semi-style": ["error"],
        "space-in-parens": ["error"],
        "space-unary-ops": ["error", {
            words: true,
            nonwords: false
        }],
        "spaced-comment": ["error"],
        "switch-colon-spacing": ["error"],
        "template-curly-spacing": ["error"],
        "template-tag-spacing": ["error", "always"],
        "yield-star-spacing": ["error"],

        /* Default base ESLint rules that we want to modify. */
        "no-constant-condition": ["error", {
            checkLoops: false
        }],

        /* Non-default TypeScript ESLint rules that we want. */
        "@typescript-eslint/array-type": ["error"],
        "@typescript-eslint/consistent-type-assertions": ["error"],
        "@typescript-eslint/explicit-function-return-type": ["error"],
        "@typescript-eslint/explicit-module-boundary-types": ["error"],
        "@typescript-eslint/no-base-to-string": ["error"],
        "@typescript-eslint/no-confusing-void-expression": ["error"],
        "@typescript-eslint/no-extraneous-class": ["error"],
        "@typescript-eslint/no-meaningless-void-operator": ["error"],
        "@typescript-eslint/no-non-null-asserted-nullish-coalescing": ["error"],
        "@typescript-eslint/no-redundant-type-constituents": ["error"],
        "@typescript-eslint/no-unnecessary-boolean-literal-compare": ["error"],
        "@typescript-eslint/no-useless-empty-export": ["error"],
        "@typescript-eslint/non-nullable-type-assertion-style": ["error"],
        "@typescript-eslint/prefer-function-type": ["error"],
        "@typescript-eslint/prefer-includes": ["error"],
        "@typescript-eslint/prefer-nullish-coalescing": ["error"],
        "@typescript-eslint/prefer-optional-chain": ["error"],
        "@typescript-eslint/prefer-readonly": ["error"],
        "@typescript-eslint/prefer-reduce-type-parameter": ["error"],
        "@typescript-eslint/prefer-regexp-exec": ["error"],
        "@typescript-eslint/prefer-string-starts-ends-with": ["error"],
        "@typescript-eslint/require-array-sort-compare": ["error", {
            ignoreStringArrays: true
        }],
        "@typescript-eslint/switch-exhaustiveness-check": ["error"],
        "@typescript-eslint/unbound-method": ["error", {
            "ignoreStatic": true
        }],
        "@typescript-eslint/unified-signatures": ["error"],

        // Code style.
        "@typescript-eslint/type-annotation-spacing": ["error"],

        /* Base ESLint rules that we turn off in favour of the TypeScript ESLint versions. */
        "default-param-last": "off",
        "dot-notation": "off",
        "no-invalid-this": "off",
        "no-loop-func": "off",
        "no-redeclare": "off",
        "no-shadow": "off",
        "no-throw-literal": "off",
        "no-unused-expressions": "off",
        "no-use-before-define": "off",
        "no-useless-constructor": "off",

        // Code style.
        "brace-style": "off",
        "comma-dangle": "off",
        "func-call-spacing": "off",
        "keyword-spacing": "off",
        "object-curly-spacing": "off",
        "quotes": "off",
        "semi": "off",
        "space-before-blocks": "off",
        "space-before-function-paren": "off",

        /* Non-default TypeScript ESLint rules that replace base ESLint rules. */
        "@typescript-eslint/default-param-last": ["error"],
        "@typescript-eslint/dot-notation": ["error"],
        "@typescript-eslint/no-invalid-this": ["error"],
        "@typescript-eslint/no-loop-func": ["error"],
        "@typescript-eslint/no-redeclare": ["error"],
        "@typescript-eslint/no-shadow": ["error"],
        "@typescript-eslint/no-throw-literal": ["error"],
        "@typescript-eslint/no-unused-expressions": ["error", {
            allowShortCircuit: true,
            allowTernary: true
        }],
        "@typescript-eslint/no-useless-constructor": ["error"],

        // Code style.
        "@typescript-eslint/brace-style": ["error", "1tbs", {
            allowSingleLine: true
        }],
        "@typescript-eslint/comma-dangle": ["error"],
        "@typescript-eslint/func-call-spacing": ["error"],
        "@typescript-eslint/keyword-spacing": ["error"],
        "@typescript-eslint/quotes": ["error", "double", {
            "avoidEscape": true
        }],
        "@typescript-eslint/semi": ["error"],
        "@typescript-eslint/object-curly-spacing": ["error"],
        "@typescript-eslint/space-before-blocks": ["error"],
        "@typescript-eslint/space-before-function-paren": ["error", {
            anonymous: "always",
            asyncArrow: "always",
            named: "never"
        }],

        /* TypeScript rules that are warnings by default. @typescript-eslint/recommended turns off incompatible base
           ESLint rules for some of these. */
        "@typescript-eslint/no-explicit-any": ["error"],
        "@typescript-eslint/no-unused-vars": ["error"],

        /* Default TypeScript ESLint rules that we don't want. */
        "@typescript-eslint/no-empty-function": "off",
        "@typescript-eslint/no-explicit-any": "off",
        "@typescript-eslint/no-inferrable-types": "off",
        "@typescript-eslint/no-namespace": "off",
        "@typescript-eslint/no-non-null-assertion": "off"
    }
};
