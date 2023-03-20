---
title: Ultra low-latency video streamer server configuration
toc: true
---

## Configuration format

The server configuration format is a JSON file. See the [server configuration reference](ServerConfigSpec/README.md),
and in particular the [`Config` interface](ServerConfigSpec/interfaces/Config.md).


## Minimal example

The following is a minimal example of a server configuration:

```javascript
{
  "video": {
    "sources": [
      "rtsp://192.0.2.2:554/video"
    ]
  }
}
```

For a first test setup, it is useful to put the `demo-client` directory in the server configuration as follows:
```json
{
  "video": {
    "sources": [
      "rtsp://192.0.2.2:554/video"
    ]
  },
  "filesystem": {
    "directories": [
      {
        "path": "/path/to/spectral-video-streamer/demo-client",
        "index": "index.html",
        "ephemeral": true
      }
    ]
  }
}
```

This makes the client available from the server. In this example, both `/index.html` and (because of the `index` field)
`/` serve the demo client.


## Defaults

The following is the default server configuration object:

```javascript
{DEFAULTS}
```
