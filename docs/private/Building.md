---
title: Building
toc: true
---


The Javascript build process runs out-of-tree by default from within the CMake build system. This page describes how to
use the Javascript build system directly.


## Basic build steps

1. Run `yarn install` from the source repository's `js` directory.
2. Run `yarn bundle` (for a production build) or `yarn bundle-dev` (for a development build) to build the server and the
   clients. This will produce a `dist` directory in `client`, `dev-client`, `demo-client`, and `server`.
3. Run `yarn foreach lint` to run the linter on everything.
4. Run `yarn docs` to build the documentation. This will produce a `dist` directory in `docs`.
5. Run `yarn test` to run the tests for the client, the server, and the shared code.

This process can be undone with `yarn clean`. The cleaning process can be verified with `git clean -dXn`.


## Building the clients

The `dev-client` and `demo-client` can both be built with `yarn bundle` and `yarn bundle-dev` from their respective
directories. In this case, the info JSON URL that determines the stream parameters and is served by the streamer server
can be fixed with `--env INFO_URL`, for example:
`yarn bundle --env INFO_URL=https://stream-demo.spectralcompute.co.uk/live/info.json`.

If this is not done, then the demo clients expect an INFO JSON GET parameter: `streaminfo`. It can be generated with,
for example:
`python -c 'import sys ; import urllib.parse ; print(urllib.parse.quote_plus(sys.argv[1]))' "https://stream-demo.spectralcompute.co.uk/live/info.json"`. The full URL would then be:
`https://stream-demo.spectralcompute.co.uk/?streaminfo=https%3A%2F%2Fstream-demo.spectralcompute.co.uk%2Flive%2Finfo.json`.
