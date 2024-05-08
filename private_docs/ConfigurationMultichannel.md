---
title: Configuration - Multichannel scenarios
toc: true
---

The server is able to stream multiple channels.
The server provides the mapping of channels at `/channelIndex.json`.
The keys in the root JSON object are paths to the channels, and the values are channel names.

## Example `server` configuration

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

## Quality autoselection logic

There's a piece of logic responsible for selecting the right quality when the channel is changed.
The general idea is to select the best quality possible, unless there was an explicit change in quality (either by the user or automatically), in which case the logic tries to find the most similar quality.
The metric of how good a quality is is quite simple and could be improved to account for things like bitrate.

## Play-pause logic and what to display on channel change

// To be specified

// Thumbnail of the current channel when switching when paused?
