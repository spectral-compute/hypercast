---
title: Server Configuration
toc: true
---

See the [`server`'s own configuration reference](../../server/docs/configuration/README.md).

For a quick guide on multichannel setups, see [Configuration - Multichannel scenarios](ServerConfigurationMultichannel.md).

There's also a possibly outdated [Server configuration](old/ServerConfiguration.md) document.

## Tell the player from `client` what to stream

The player takes configuration where its behavior regarding what to stream can be specified.
That includes turning the channel index on or off, and specifying the default channel to stream.
For details, refer to the in-code documentation of the `Player` class in the `client` library.

## Telling `dev-client` where to find the stream

`dev-client` uses URL query parameters to specify the video source.
This can be useful when working with multiple servers.

Here's an example of the link to open in the browser if the `dev-client` is started on `localhost:8000` and the server is on `localhost:8001`:

    http://localhost:8000/?server=http://localhost:8001

## Telling `demo-client` where to find the stream

demo-client takes REACT_APP_STREAM_SERVER environment variable to specify the server address.

Here's an example of the command to start the demo-client if the server is on `localhost:8001`:

    REACT_APP_STREAM_SERVER=http://localhost:8001 yarn start

## Channel list polling

There was a feature of polling the channel index from the server.
However, this functionality was disabled because Cloudflare charges per request.
If this functionality is something that we want to reimplement, we should take a look at these things:
- [Long polling](https://javascript.info/long-polling)
- [Server-sent events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events)
