---
title: Ultra low-latency video streamer server configuration fields
toc: true
toc-depth: 6
---

This page describes the fields available in the configuration format.

Except for those fields whose default is described as "*Required*", all parameters are optional. Where a fixed default
exists, it is described. Otherwise, the default is computed based on other settings and the input video.

| Field         | Default | Type   | Description                                         |
|---------------|---------|--------|-----------------------------------------------------|
| `channels`    |         | Object | The video (and audio) source.                       |
| `directories` |         | Object | A set of directories to expose via the HTTP server. |
| `network`     |         | Object | Lower level network configuration.                  |
| `http`        |         | Object | HTTP server configuration.                          |
| `log`         |         | Object | How to do logging.                                  |
| `features`    |         | Object | Enable and disable server features.                 |


## `channels`

The keys of the `channels` object are base paths to export on the server for the channel. For example, if set to
`/live`, then the information JSON resource will be `/live/info.json`. The value for each key should be an object with
the following keys:

| Field       | Default    | Type             | Description                                    |
|-------------|------------|------------------|------------------------------------------------|
| `source`    | *Required* | Object or string | The video (and audio) source.                  |
| `qualities` |            | Array of objects | The set of qualities to make available.        |
| `dash`      |            | Object           | Settings related to DASH.                      |
| `history`   |            | Object           | What historical information to keep available. |
| `name`      |            | String           | The human-readable name to give the channel.   |


### `channels.source`

If a string, then this is interpreted the as the `url` field.

| Field       | Default    | Type             | Description                                               |
|-------------|------------|------------------|-----------------------------------------------------------|
| `url`       | *Required* | String           | The URL to give to FFMPEG's `-i` input.                   |
| `arguments` |            | Array of strings | Additional arguments to pass to FFMPEG for this input.    |
| `loop`      | False      | Boolean          | Whether to loop this source indefinitely.                 |
| `timestamp` | False      | Integer          | Whether to superimpose a timestamp onto the video stream. |
| `latency`   |            | Integer          | The latency from realtime of the source.                  |


#### `channels.source.latency`

Some sources, such as cameras, can have a significant latency of their own. This setting is used along with the target
latency setting of `qualities` to set other latency-sensitive parameter defaults. The default value for this is set
based on the source type.


### `channels.qualities`

Each element of this array specifies the quality for a given stream. Each quality represents a separate interleaved
audio/video stream.

| Field                         | Default | Type    | Description                                                      |
|-------------------------------|---------|---------|------------------------------------------------------------------|
| `video`                       |         | Object  | The video stream configuration for this quality setting.         |
| `audio`                       |         | Object  | The audio stream configuration for this quality setting.         |
| `targetLatency`               | 2000    | Integer | Target latency, in ms.                                           |
| `minInterleaveRate`           |         | Integer | The minimum bitrate in kBit/s for this quality.                  |
| `minInterleaveWindow`         |         | Integer | The window, in ms, to evaluate the minimum interleave rate over. |
| `interleaveTimestampInterval` | 100     | Integer | Average period, in ms, between timestamps in an interleave.      |
| `clientBufferControl`         |         | Object  | Settings for client-side buffer control.                         |


#### `channels.qualities.video`

| Field                     | Default | Type                               | Description                          |
|---------------------------|---------|------------------------------------|--------------------------------------|
| `width`                   |         | Integer                            | The width of the video stream.       |
| `height`                  |         | Integer                            | The height of the video stream.      |
| `frameRate`               |         | `[`Integer`,` Integer`]` or String | The frame rate to use for the video. |
| `bitrate`                 |         | Integer                            | The bitrate limit in kBit/s.         |
| `minBitrate`              |         | Integer                            | The minimum bitrate in kBit/s.       |
| `crf`                     | 25      | Integer                            | Constant rate factor.                |
| `rateControlBufferLength` |         | Integer                            | Rate control buffer length in ms.    |
| `codec`                   | `h264`  | String                             | The video codec to use.              |
| `h26xPreset`              |         | String                             | Preset to use for H.264/H.265.       |
| `vpXSpeed`                | 8       | Integer                            | Speed to use for VP8/VP9/AV1.        |
| `gopsPerSegment`          | 1       | Integer                            | Number of (forced) GOPs per segment. |


##### `channels.qualities.video.width`

By default, the resolution of the input will be used. If only one of width and height is given, then the other will be
calculated from the aspect ratio of the input video.


##### `channels.qualities.video.height`

By default, the resolution of the input will be used. If only one of width and height is given, then the other will be
calculated from the aspect ratio of the input video.


##### `channels.qualities.video.frameRate`

By default, the frame rate of the input video is used. Use the special value of `half` to use half the frame rate of the
input video, or `half+` to halve the frame rate if the result would be at least 23 fps.


##### `channels.qualities.video.bitrate`

By default, this is chosen based on the resolution, frame rate, and CRF.


##### `channels.qualities.video.minBitrate`

This is useful for force-purging CDN buffers. There are trade-offs between bandwidth required, and latency in the
presence of buffers. This feature can be disabled entirely by setting to 0.

Not all codecs support this. Currently, only h264 and h265 do.


##### `channels.qualities.video.crf`

For H.264, see the [CRF section of the Ffmpeg h264 encoding guide](https://trac.ffmpeg.org/wiki/Encode/H.264#crf). Other
codecs have different CRF characteristics.


##### `channels.qualities.video.codec`

| Value  | Description                        |
|--------|------------------------------------|
| `h264` | [H.264](https://caniuse.com/mpeg4) |
| `h265` | [H.265](https://caniuse.com/hevc)  |
| `vp8`  | [VP8](https://caniuse.com/webm)    |
| `vp9`  | [VP9](https://caniuse.com/webm)    |
| `av1`  | [AV1](https://caniuse.com/av1)     |


##### `channels.qualities.video.h26xPreset`

The H.264/H.265 preset. Several sections of [FFMPEG's H.264 page](https://trac.ffmpeg.org/wiki/Encode/H.264) describe
these. You should use the slowest preset that works reliably for your application for best quality for a given bitrate.

| Value       |
|-------------|
| `ultrafast` |
| `superfast` |
| `veryfast`  |
| `faster`    |
| `fast`      |
| `medium`    |
| `slow`      |
| `slower`    |
| `veryslow`  |
| `placebo`   |


##### `channels.qualities.video.vpXSpeed`

This sets `ffmpeg`'s `-speed` option for VP8/VP9/AV1. It should be between 0 and 8. Higher numbers are faster. You
should use the smallest value that works reliably for your application for best quality for a given bitrate.


##### `channels.qualities.video.gopsPerSegment`

Forced keyframes are inserted at regular intervals. There must be one at the start of each segment. This parameter can
be used to force a shorter GOP (by increasing the number of GOPs per segment), but otherwise it should be left to its
default.


#### `channels.qualities.audio`

| Field        | Default | Type    | Description             |
|--------------|---------|---------|-------------------------|
| `sampleRate` |         | Integer | The sample rate in Hz.  |
| `bitrate`    | 64      | Integer | The bitrate in kBit/s.  |
| `codec`      | `aac`   | String  | The audio codec to use. |


##### `channels.qualities.audio.sampleRate`

By default, the sample rate is calculated by applying each of the following conditions in turn. Conditions that cannot
be satisfied along with their preceding conditions are ignored.

1. The sample rate is chosen to be compatible with the codec.
2. The sample rate is chosen to be no greater than 48 kHz.
3. The sample rate is chosen to be no greater than the input sample rate.
4. The sample rate is chosen to be the greatest integer divisor of the input sample rate that's at least 32 kHz. This is
   meant to, for example: choose 48 kHz if the input is 96 kHz.
5. The sample rate is chosen to be the highest out of those rates not excluded by the preceding conditions.


##### `channels.qualities.audio.codec`

| Value  | Description                      |
|--------|----------------------------------|
| `none` | No audio.                        |
| `aac`  | [AAC](https://caniuse.com/aac)   |
| `opus` | [Opus](https://caniuse.com/opus) |

Note that the audio codec has to be set explicitly to `none` if the input has no audio. This serves as a protection
against accidentally streaming without audio.


#### `channels.qualities.targetLatency`

There is a trade-off between achieving lower latency and smooth playback.


#### `channels.qualities.segmentDuration`

It is recommended that the segments be approximately 8000 to 16000 ms in duration. Shorter segments allow clients to get
onto the stream more quickly as the client must download the entire segment up to the live edge, but longer segments
provide for more efficient compression.


#### `channels.qualities.minInterleaveRate`

If the specified bitrate is not achieved naturally, then the interleave is padded with extra data to achieve the minimum
rate. This feature can be disabled entirely by setting to 0.


#### `channels.qualities.minInterleaveWindow`

The rate is evaluated twice per window, so the worst case scenario is no data transmitted for 1.5x the window.


#### `channels.qualities.transitBufferSize`

This is used to correct for things like a CDN buffer that does not forward data until full when computing other
parameters (such as minimum bitrates).


#### `channels.qualities.interleaveTimestampInterval`

The timestamps are used for buffer control.


#### `channels.qualities.clientBufferControl`

| Field             | Type    | Description                                                                       |
|-------------------|---------|-----------------------------------------------------------------------------------|
| `minBuffer`       | Integer | Minimum buffer to allocate, even if this is more than the observed requirement.   |
| `extraBuffer`     | Integer | Extra buffer above the maximum observed when seeking.                             |
| `initialBuffer`   | Integer | The size of the target buffer when a stream is first played, in ms.               |
| `seekBuffer`      | Integer | The buffer to keep when seeking to the live edge, in ms.                          |
| `minimumInitTime` | Integer | The minimum time to wait before doing the initial seek to get the stream playing. |


##### `channels.qualities.clientBufferControl.minBuffer`

This parameter accounts for the various sources of jitter. By default, this is set to cope with the expected sources of
jitter and the extra buffer. Increasing this parameter makes playback smoother at the expense of latency.

This parameter does not apply before the buffer history is built, when
`channels.qualities.clientBufferControl.initialBuffer` parameter applies instead.


##### `channels.qualities.clientBufferControl.extraBuffer`

Increasing this parameter makes playback smoother at the expense of latency.


##### `channels.qualities.clientBufferControl.initialBuffer`

If this buffer is exceeded before a buffer history is built, it's considered that the player has fallen behind.


##### `channels.qualities.clientBufferControl.seekBuffer`

Increasing this can prevent additional stalls after a seek and after starting to play a stream.


##### `channels.qualities.clientBufferControl.minimumInitTime`

Increasing this reduces stuttering at the start at the expense of delaying the video playback. It should not adversely
affect latency once playback has begun.


### `channels.dash`

| Field                 | Default | Type    | Description                                                    |
|-----------------------|---------|---------|----------------------------------------------------------------|
| `segmentDuration`     | 15000   | Integer | The average segment duration, in ms.                           |
| `expose`              | False   | Boolean | Whether to make a standard DASH stream available.              |
| `preAvailabilityTime` | 4000    | Integer | Time, in ms, before the start of a segment to accept requests. |


#### `channels.dash.expose`

This can be enabled to allow the underlying DASH manifest and streams to be downloaded. This is a simple solution to
making the stream available to standard DASH players. It can also be used to easily access the raw segments for
debugging.

Note: Turning this on to access both DASH streams and ultra low-latency streams means the server has to upload the video
twice.


#### `channels.dash.preAvailabilityTime`

Due to HTTP cache timing resolution limitations, this should be greater than 1000.


### `channels.history`

| Field               | Default | Type    | Description                                                               |
|---------------------|---------|---------|---------------------------------------------------------------------------|
| `historyLength`     | 90      | Integer | The amount of time, in seconds, to make historical segments available.    |
| `persistentStorage` |         | String  | Where to store the DASH files permanently. They're not stored if not set. |


## `directories`

This is useful to easily build a basic streaming server that serves a client. For a more complex case, such as with
server-side scripting, authentication, cookies, or session management, the use of a reverse HTTP proxy like Nginx is
recommended.

The name of each field of this object is a path to expose on the server. The values of each field are either an object
as follows, or a string that is interpreted as the `localPath` field:

| Field             | Default    | Type    | Description                                                               |
|-------------------|------------|---------|---------------------------------------------------------------------------|
| `localPath`       | *Required* | String  | The location on the local server where the directory resides.             |
| `index`           |            | String  | Path within the directory to use if the directory itself is requested.    |
| `secure`          | False      | Boolean | If this directory is accessible only in secure contexts (e.g: localhost). |
| `ephemeral`       | False      | Boolean | If the cache control should be ephemeral.                                 |
| `maxWritableSize` |            | Integer | Maximum size in MiB that can be PUT. If not given, PUT is not allowed.    |


### `directories.ephemeral`

Ephemeral caching sets a very short (e.g: 1 s) cache timeout. This is useful for data that might need to be updated at
short notice. It's also useful for debugging.


## `network`

Low level (or at least low-ish level) network configuration.

| Field               | Default | Type           | Description                                                         |
|---------------------|---------|----------------|---------------------------------------------------------------------|
| `port`              | 8080    | Integer        | The TCP port to listen on.                                          |
| `publicPort`        |         | Integer        | The TCP port to listen on only allowing access to public resources. |
| `privateNetworks`   | `[]`    | `[` String `]` | The set of networks to consider private, excluding localhost.       |
| `transitLatency`    | 50      | Integer        | Network latency, in ms, to assume, excluding `transitBufferSize`.   |
| `transitJitter`     | 200     | Integer        | Network jitter, in ms, to assume, excluding `transitBufferSize`.    |
| `transitBufferSize` | 32768   | Integer        | Buffer size, in bytes, to assume during transit.                    |


### `network.publicPort`

The server that listens on the port specified by `network.port` accepts connections from both public and private
contexts (the latter of which is used for things like the connections from `ffmpeg`, and the API used by the settings
UI). If using a proxy that connects to the server from `localhost`, it would not be possible to distinguish between
public and private requests based on IP address. If `network.publicPort` is given, the server will also listen on that
port, but will only serve public resources to connections to that port.


### `network.privateNetworks`

Localhost addresses are always considered private on the port given by `network.port` (and never on the port given by
`network.publicPort`), regardless of this setting. This setting adds networks to consider private (and therefore able to
access the API and DASH segments) when accessed via port `network.port`. Acceptable values include:

 - IPv4 addresses (e.g: `192.0.2.2`).
 - IPv4 networks (e.g: `192.0.2.2/24`).
 - IPv6 addresses (e.g: `2001:db8:c0de::2`).
 - IPv6 networks (e.g: `2001:db8:c0de::2/64`).

The setting may also be a single string rather than an array of strings.


## `http`

Server-wide HTTP-specific configuration.

| Field                   | Default | Type             | Description                                                  |
|-------------------------|---------|------------------|--------------------------------------------------------------|
| `origin`                | `*`     | String or `null` | The value for the `Access-Control-Allow-Origin` HTTP header. |
| `cacheNonLiveTime`      | 600     | Integer          | Time, in seconds, to cache non-live data.                    |
| `ephemeralWhenNotFound` |         | String `[]`      | Paths that produce ephemeral 404s when they don't exist.     |


### `http.origin`

The [Access-Control-Allow-Origin](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Access-Control-Allow-Origin)
header describes what URLs the scripts that access this server are allowed to have. It's enforced by the web browser. In
a production environment, this should be set more restrictively than the default. If set to `null`, this header will not
be sent.


### `http.ephemeralWhenNotFound`

Some resources, such as the `info.json` resources for each channel can appear at short notice. Most non-existing paths
return a 404 Not Found error with 10 minute caching, but for resources such as `info.json`, this is problematic. This
field can be used to set the caching for specified resources to a small value.


## `log`

| Field   | Default | Type    | Description                                                                         |
|---------|---------|---------|-------------------------------------------------------------------------------------|
| `path`  |         | String  | The file to log to. If not set, logging is in memory and printed to standard error. |
| `print` |         | Boolean | Whether to print the log to standard error.                                         |
| `level` | `info`  | Boolean | Minimum log level, out of: `debug`, `info`, `warning`, `error`, and `fatal`.        |


## `features`

| Field          | Default | Type    | Description                                      |
|----------------|---------|---------|--------------------------------------------------|
| `channelIndex` | True    | Boolean | Enable the channel index (`/channelIndex.json`). |
