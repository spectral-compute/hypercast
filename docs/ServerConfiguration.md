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


## Defaults

The following is the default server configuration object:

```javascript
{DEFAULTS}
```
