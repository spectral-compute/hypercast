---
title: Production setup
---

The `server` uses CMake build system with XCMake extension (make sure to `git submodule update --checkout --init --recursive`).

Successful build can be achieved by trial and error.
The only thing that needs to be specified is `CMAKE_INSTALL_PREFIX`.

## Building JS as part of the server build

Look out for situations when you build JS packages of the project in their folders as that can eventually lead to `.yarn/` getting poisoned which can cause build problems.
`yarn clean` or `rm -rf .yarn/ && git restore .yarn/` may help in such situations.

There is a `js.cmake` that is responsible for building JS packages as part of the server build (which happens by default). JS building can be disabled with a CMake argument on configuration.

## What to do next

After the server has been built and the JS packages have been bundled, you can find the server executable in your `CMAKE_INSTALL_PREFIX` and start it with some configuration file.

As the server can serve static files, it can be used to serve the JS packages (e.g. `demo-client`).
Note, though, that there may be problems when trying to host single-page applications like that among other things.

Refer to [Configuration](Configuration.md) for details.
