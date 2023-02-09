---
title: Ultra low-latency video streamer server configuration
toc: true
---

The server configuration format is a JSON file. This page describes the server configuration format at a high level and
with examples.

The following pages describing the configuration format in more detail exist:

 - A detailed description of the [configuration fields](./Fields.md).


## Examples


### Minimal

```json
{
  "source": "rtsp://192.0.2.3/"
}
```


### Serving a client with two qualities and a superimposed timestamp

```json
{
  "source": {
    "url": "rtsp://192.0.2.3/",
    "timestamp": true
  },
  "qualities": [
    {
      "video": {
        "width": 1920,
        "height": 1080
      }
    },
    {
      "video": {
        "width": 1280,
        "height": 720
      },
      "audio": {
        "bitrate": 32
      }
    }
  ],
  "paths": {
    "directories": {
      "/": {
        "localPath": "/srv/streamer-client",
        "index": "index.html"
      }
    }
  }
}
```
