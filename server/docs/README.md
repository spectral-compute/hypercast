---
title: Ultra low-latency video streamer server
toc: false
---

## Configuration

This page describes how to configure the video streamer server.

 - [Server configuration](./configuration/README.md)

## Building

Initialise submodules:
```bash
git submodule update --init --recursive
```

Ensure you have local installations of:
- Boost (extra/boost pacman package)
- nlohmann_json (pacman: community/nlohmann-json)
- FFmpeg (run `build-ffmpeg --help` from the install tree for information about how to get a suitable build)

To compile the documentation, ensure you have installed:
- pandoc (pacman: community/pandoc)
- doxygen (pacman: extra/doxygen)
- graphviz (pacman: extra/graphviz)

Build with CMake

## Running

Run the server, passing the path of your config file (see [above](#configuration)) as the only argument.
