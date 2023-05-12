---
title: Project structure
---

The project consists of the following parts:

- `client`: A TypeScript library that provides a client for the server.
  It has two main responsibilities:
  - Handle incoming video streams and shove them into a `<video>` element.
  - Operate the DOM elements that make up the UI according to its config and the state of the server.
- `common`: Type definitions.
- `demo-client`: A React application that uses the `client` library.
  It is used as a demonstration of RISE, showing how to use the libraries we provide along with showing the player in action.
  - This package currently contains a React component for the player which is to be extracted into a separate package.
- `dev-client`: A static page (hosted with python `http.server`) that uses the `client` library.
  It is used for development of the `client` library and displays debug information.
- `server`: A server written in C++ and responsible for encoding the media streams.
  It can also serve static files and receive commands (e.g. to update its config in runtime).
  - [`server`'s own documentation](../../server/docs/README.md)
- `settings-ui`: A UI for configuring servers.

There are also some extra things:

- `interjection-tools` ([Read more](interjection-tools/README.md))
- `ffmpeg-tools` - Used to build ffmpeg with features that we need.
  Use `build-ffmpeg --help` from the install tree to learn more.
  The ffmpeg should be added to `PATH` for the server to find it.
  The built ffmpeg depends on its own libraries, which it tries to find using a relative path.
  This means that the ffmpeg's install folder can be moved around as a whole, but the internal structure should be preserved.
