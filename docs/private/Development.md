---
title: Development
---

## Hot reload and other development features

`demo-client` is a React app that supports hot reloading.
This means that as long as a start script is running, it is going to automatically rebuild and reload the app when the source code changes, even preserving the application state when possible.

This functionality is builtin when we are talking about changes in `demo-client` itself and its dependencies.
However, it depends on the built versions of the dependencies, so we need to actuall rebuild the dependencies when we change them.
In a simple case of a typescript library that can be achieved with `tsc -w`.

This is also a little more difficult when we are talking about running the apps that don't have hot reloading by themselves, because directories to watch will need to be specified manually.

### How to run `demo-client` with hot reloading

1. Make sure your dependencies are installed (`yarn` or `yarn install`).
1. Run `yarn demo-client:dev` to start the `demo-client` with hot reloading including its dependencies.

Be wary of possible problems with `yarn`'s cache when building the server after doing that.

## Neat things

- Did you know that yarn scripts that contain a colon (`:`) can be ran from anywhere in the project, even if they are defined in a package?
  The package just needs to be included into the workspace.
