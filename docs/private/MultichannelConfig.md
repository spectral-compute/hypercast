---
title: Configuring the server to stream multiple channels
toc: true
---

The server is able to stream multiple channels.
The server provides the mapping of channels at `/channelsIndex.json`.
The keys in the root JSON object are paths to the channels, and the values are channel names.

This is an example of configuration that can be used to make the server stream multiple channels.
Note that the persistency directories must be different for each channel.

```json
{
    "channels": {
        "/live1": {
            "source": {
                "url": "<path to a video>",
                "loop": true
            },
            "qualities": [
                {
                    "targetLatency": 1000
                }
            ],
            "history": {
                "persistentStorage": "<path to a persistency directory>"
            }
        },
        "/live2": {
            "source": {
                "url": "<path to a different video>",
                "loop": true
            },
            "qualities": [
                {
                    "targetLatency": 1000
                }
            ],
            "history": {
                "persistentStorage": "<path to a different persistency directory>"
            }
        }
    },
    "network": {
        "port": 8001
    }
}

```

Such configuration will yield the following `/channelIndex.json`:

```json
{
    "/live1/info.json": null,
    "/live2/info.json": null
}
```

## Using the player with this configuration

The player can be then fed a link to the channel index.
The player will then display the only channel if there's only one of them, and won't show a channel selector.
Otherwise, the first channel will be displayed.

Logic subject to change.
Maybe the player would support both options, taking a channel index URL for multi-channel scenarios, and a single channel info URL for single-channel scenarios.

## dev-client

If using dev-client, the full link could be:

    http://localhost:8000/?server=http://localhost:8001

## demo-client

demo-client takes REACT_APP_STREAM_SERVER environment variable to specify the server address.
demo-server could be started with:

    REACT_APP_STREAM_SERVER=http://localhost:8001 yarn start

## Play-pause logic and what to display on channel change

// To be specified

// Thumbnail of the current channel when switching when paused?

## Channel list polling

// To be filled out
