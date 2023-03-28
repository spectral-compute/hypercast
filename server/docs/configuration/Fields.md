---
title: Ultra low-latency video streamer server configuration fields
toc: true
toc-depth: 6
---

This page describes the fields available in the configuration format.

Except for those fields whose default is described as "*Required*", all parameters are optional. Where a fixed default
exists, it is described. Otherwise, the default is computed based on other settings and the input video.


| Field       | Default    | Type             | Description                                    |
|-------------|------------|------------------|------------------------------------------------|
| `source`    | *Required* | Object or string | The video (and audio) source.                  |
| `qualities` |            | Array of objects | The set of qualities to make available.        |
| `dash`      |            | Object           | Settings related to DASH.                      |
| `network`   |            | Object           | Lower level network configuration.             |
| `http`      |            | Object           | HTTP server configuration.                     |
| `paths`     |            | Object           | Layout of paths for the exposed HTTP server.   |
| `history`   |            | Object           | What historical information to keep available. |
| `log`       |            | Object           | How to do logging.                             |


## `source`

If a string, then this is interpreted the as the `url` field.

| Field       | Default    | Type             | Description                                               |
|-------------|------------|------------------|-----------------------------------------------------------|
| `url`       | *Required* | String           | The URL to give to FFMPEG's `-i` input.                   |
| `arguments` |            | Array of strings | Additional arguments to pass to FFMPEG for this input.    |
| `loop`      | False      | Boolean          | Whether to loop this source indefinitely.                 |
| `timestamp` | False      | Integer          | Whether to superimpose a timestamp onto the video stream. |
| `latency`   |            | Integer          | The latency from realtime of the source.                  |


### `latency`

Some sources, such as cameras, can have a significant latency of their own. This setting is used along with the target
latency setting of `qualities` to set other latency-sensitive parameter defaults. The default value for this is set
based on the source type.


## `qualities`

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


### `qualities.video`

| Field                     | Default  | Type                               | Description                          |
|---------------------------|----------|------------------------------------|--------------------------------------|
| `width`                   |          | Integer                            | The width of the video stream.       |
| `height`                  |          | Integer                            | The height of the video stream.      |
| `frameRate`               |          | `[`Integer`,` Integer`]` or String | The frame rate to use for the video. |
| `bitrate`                 |          | Integer                            | The bitrate limit in kBit/s.         |
| `minBitrate`              |          | Integer                            | The minimum bitrate in kBit/s.       |
| `crf`                     | 25       | Integer                            | Constant rate factor.                |
| `rateControlBufferLength` |          | Integer                            | Rate control buffer length in ms.    |
| `codec`                   | `h264`   | String                             | The video codec to use.              |
| `h26xPreset`              | `faster` | String                             | Preset to use for H.264/H.265.       |
| `vpXSpeed`                | 8        | Integer                            | Speed to use for VP8/VP9/AV1.        |
| `gop`                     |          | Integer                            | Group of pictures size in frames.    |


#### `qualities.video.width`

By default, the resolution of the input will be used. If only one of width and height is given, then the other will be
calculated from the aspect ratio of the input video.


#### `qualities.video.height`

By default, the resolution of the input will be used. If only one of width and height is given, then the other will be
calculated from the aspect ratio of the input video.


#### `qualities.video.frameRate`

By default, the frame rate of the input video is used. Use the special value of `half` to use half the frame rate of the
input video, or `half+` to halve the frame rate if the result would be at least 23 fps.


#### `qualities.video.bitrate`

By default, this is chosen based on the resolution, frame rate, and CRF.


#### `qualities.video.minBitrate`

This is useful for force-purging CDN buffers. There are trade-offs between bandwidth required, and latency in the
presence of buffers. This feature can be disabled entirely by setting to 0.

Not all codecs support this. Currently, only h264 and h265 do.


#### `qualities.video.crf`

For H.264, see the [CRF section of the Ffmpeg h264 encoding guide](https://trac.ffmpeg.org/wiki/Encode/H.264#crf). Other
codecs have different CRF characteristics.


#### `qualities.video.codec`

| Value  | Description                        |
|--------|------------------------------------|
| `h264` | [H.264](https://caniuse.com/mpeg4) |
| `h265` | [H.265](https://caniuse.com/hevc)  |
| `vp8`  | [VP8](https://caniuse.com/webm)    |
| `vp9`  | [VP9](https://caniuse.com/webm)    |
| `av1`  | [AV1](https://caniuse.com/av1)     |


#### `qualities.video.h26xPreset`

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


#### `qualities.video.vpXSpeed`

This sets `ffmpeg`'s `-speed` option for VP8/VP9/AV1. It should be between 0 and 8. Higher numbers are faster. You
should use the smallest value that works reliably for your application for best quality for a given bitrate.


#### `qualities.video.gop`

A segment must be an integer multiple of this number of frames. If this is chosen so that the average segment duration
is more than a few hundred ms off of an integer multiple of this, then the client may struggle to maintain
synchronization with the stream. By default, this parameter is chosen to make each segment one GOP in length. This
parameter can be used to force a shorter GOP, but otherwise it should be left to its default.


### `qualities.audio`

| Field        | Default | Type    | Description             |
|--------------|---------|---------|-------------------------|
| `sampleRate` |         | Integer | The sample rate in Hz.  |
| `bitrate`    | 64      | Integer | The bitrate in kBit/s.  |
| `codec`      | `aac`   | String  | The audio codec to use. |


#### `qualities.video.width`

By default, the sample rate is calculated by applying each of the following conditions in turn. Conditions that cannot
be satisfied along with their preceding conditions are ignored.

1. The sample rate is chosen to be compatible with the codec.
2. The sample rate is chosen to be no greater than 48 kHz.
3. The sample rate is chosen to be no greater than the input sample rate.
4. The sample rate is chosen to be the greatest integer divisor of the input sample rate that's at least 32 kHz. This is
   meant to, for example: choose 48 kHz if the input is 96 kHz.
5. The sample rate is chosen to be the highest out of those rates not excluded by the preceding conditions.


#### `qualities.audio.codec`

| Value  | Description                      |
|--------|----------------------------------|
| `none` | No audio.                        |
| `aac`  | [AAC](https://caniuse.com/aac)   |
| `opus` | [Opus](https://caniuse.com/opus) |

Note that the audio codec has to be set explicitly to `none` if the input has no audio. This serves as a protection
against accidentally streaming without audio.


### `qualities.targetLatency`

There is a trade-off between achieving lower latency and smooth playback.


### `qualities.segmentDuration`

It is recommended that the segments be approximately 8000 to 16000 ms in duration. Shorter segments allow clients to get
onto the stream more quickly as the client must download the entire segment up to the live edge, but longer segments
provide for more efficient compression.


### `qualities.minInterleaveRate`

If the specified bitrate is not achieved naturally, then the interleave is padded with extra data to achieve the minimum
rate. This feature can be disabled entirely by setting to 0.


### `qualities.minInterleaveWindow`

The rate is evaluated twice per window, so the worst case scenario is no data transmitted for 1.5x the window.


### `qualities.transitBufferSize`

This is used to correct for things like a CDN buffer that does not forward data until full when computing other
parameters (such as minimum bitrates).


### `qualities.interleaveTimestampInterval`

The timestamps are used for buffer control.


### `qualities.clientBufferControl`

| Field             | Type    | Description                                                                       |
|-------------------|---------|-----------------------------------------------------------------------------------|
| `extraBuffer`     | Integer | Extra buffer above the maximum observed when seeking.                             |
| `initialBuffer`   | Integer | The size of the target buffer when a stream is first played, in ms.               |
| `seekBuffer`      | Integer | The buffer to keep when seeking to the live edge, in ms.                          |
| `minimumInitTime` | Integer | The minimum time to wait before doing the initial seek to get the stream playing. |


#### `qualities.clientBufferControl.extraBuffer`

Increasing this parameter makes playback smoother at the expense of latency.


#### `qualities.clientBufferControl.initialBuffer`

If this buffer is exceeded before a buffer history is built, it's considered that the player has fallen behind.


#### `qualities.clientBufferControl.seekBuffer`

Increasing this can prevent additional stalls after a seek and after starting to play a stream.


#### `qualities.clientBufferControl.minimumInitTime`

Increasing this reduces stuttering at the start at the expense of delaying the video playback. It should not adversely
affect latency once playback has begun.


## `dash`

| Field                 | Default | Type    | Description                                                    |
|-----------------------|---------|---------|----------------------------------------------------------------|
| `segmentDuration`     | 15000   | Integer | The average segment duration, in ms.                           |
| `expose`              | False   | Boolean | Whether to make a standard DASH stream available.              |
| `preAvailabilityTime` | 4000    | Integer | Time, in ms, before the start of a segment to accept requests. |


### `dash.expose`

This can be enabled to allow the underlying DASH manifest and streams to be downloaded. This is a simple solution to
making the stream available to standard DASH players. It can also be used to easily access the raw segments for
debugging.

Note: Turning this on to access both DASH streams and ultra low-latency streams means the server has to upload the video
twice.


### `dash.preAvailabilityTime`

Due to HTTP cache timing resolution limitations, this should be greater than 1000.


## `network`

Low level (or at least low-ish level) network configuration.

| Field               | Default | Type    | Description                                                       |
|---------------------|---------|---------|------------------------------------------------------------- -----|
| `port`              | 8080    | Integer | The TCP port to listen on.                                        |
| `transitLatency`    | 50      | Integer | Network latency, in ms, to assume, excluding `transitBufferSize`. |
| `transitJitter`     | 200     | Integer | Network jitter, in ms, to assume, excluding `transitBufferSize`.  |
| `transitBufferSize` | 32768   | Integer | Buffer size, in bytes, to assume during transit.                  |


## `http`

Server-wide HTTP-specific configuration.

| Field                 | Default | Type             | Description                                                    |
|-----------------------|---------|------------------|----------------------------------------------------------------|
| `origin`              | `*`     | String or `null` | The value for the `Access-Control-Allow-Origin` HTTP header.   |
| `cacheNonLiveTime`    | 600     | Integer          | Time, in seconds, to cache non-live data.                      |


### `http.origin`

The [Access-Control-Allow-Origin](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Access-Control-Allow-Origin)
header describes what URLs the scripts that access this server are allowed to have. It's enforced by the web browser. In
a production environment, this should be set more restrictively than the default. If set to `null`, this header will not
be sent.


## `paths`

| Field         | Default           | Type   | Description                                         |
|---------------|-------------------|--------|-----------------------------------------------------|
| `liveInfo`    | `/live/info.json` | String | The path to the live stream information JSON.       |
| `liveStream`  | `/live/{uid}`     | String | Path to the live stream files.                      |
| `directories` |                   | Object | A set of directories to expose via the HTTP server. |


### `paths.liveStream`

If the string `{uid}` appears in this path, it is replaced by a string that is unique to every server startup.


### `paths.directories`

This is useful to easily build a basic streaming server that serves a client. For a more complex case, such as with
server-side scripting, authentication, cookies, session management, or where multiple video sources are required from
the same server, the use of a reverse HTTP proxy like Nginx is recommended.

The name of each field of this object is a path to expose on the server. The values of each field are either an object
as follows, or a string that is interpreted as the `localPath` field:

| Field       | Default    | Type    | Description                                                                    |
|-------------|------------|---------|--------------------------------------------------------------------------------|
| `localPath` | *Required* | String  | The location on the local server where the directory resides.                  |
| `index`     |            | String  | Path within the directory to use if the directory itself is requested.         |
| `secure`    | False      | Boolean | Whether this directory is accessible only in secure contexts (e.g: localhost). |
| `ephemeral` | False      | Boolean | Whether the cache control should be ephemeral.                                 |


#### `paths.directories.ephemeral`

Ephemeral caching sets a very short (e.g: 1 s) cache timeout. This is useful for data that might need to be updated at
short notice. It's also useful for debugging.


## `history`

| Field               | Default | Type    | Description                                                               |
|---------------------|---------|---------|---------------------------------------------------------------------------|
| `historyLength`     | 90      | Integer | The amount of time, in seconds, to make historical segments available.    |
| `persistentStorage` |         | String  | Where to store the DASH files permanently. They're not stored if not set. |


## `log`

| Field   | Default | Type    | Description                                                                         |
|---------|---------|---------|-------------------------------------------------------------------------------------|
| `path`  |         | String  | The file to log to. If not set, logging is in memory and printed to standard error. |
| `print` |         | Boolean | Whether to print the log to standard error.                                         |
| `level` | `info`  | Boolean | Minimum log level, out of: `debug`, `info`, `warning`, `error`, and `fatal`.        |
