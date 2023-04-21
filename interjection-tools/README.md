# Interjection Tools

The video streamer supports interjections. An interjection is a (usually short) video that can be made to play on-cue
instead of the main stream. It is possible to make different videos play for different clients at the same time.
Interjections are useful for targeted advertisements, regional announcements and weather reports, and other short
differentiated content that is to be inserted into a stream.

Interjections can be combined into sets of interjections from which the client should choose what to play. These are
called interjection sets.


## Example usage

Starting with an empty directory called `interjections` into which the encoded interjections will be placed, a set
videos to turn into interjections: `src1.mkv` and `src2.mkv`, and corresponding metadata `src1.json` and `src2.json`
which is intended to be used to choose between interjections, the following commands fill the `interjections` directory
so that `src1.mkv` and `src2.mkv` can be served as interjections by exposing the `interjections` directory via an HTTP
CDN and sending a URL to the `interjection-set.json` file inside it to the client.

1. `encode-interjection interjections/src1 src1.mkv` Encodes `src1` as an interjection.
2. `set-interjection-metadata interjections/src1 -t src1.json` Sets the metadata to decide whether to choose `src1.mkv`
   as the interjection to play to that contained in `src1.json`. This overwrites
   `interjections/src1/interjection.json` with the new metadata.
3. `encode-interjection interjections/src2 src2.mkv`
4. `set-interjection-metadata interjections/src2 -t src2.json`
5. `generate-interjection-set-metadata interjections` Generates metadata describing the entire interjection-set. This
   creates `interjections/interjection-set.json`.


## `encode-interjection`

The `encode-interjection` program encodes a video to a format that is suitable for use as an interjection, including the
corresponding technical metadata.

The output of the program is a directory containing files related to the interjection. Each interjection should have its
own directory. Metadata that can be used to drive the choice between interjections can be inserted with the
`set-interjection-metadata` tool, and sets of interjections can be created with `generate-interjection-set-metadata`.


### Configuration

It's useful to convert many interjections with the same configuration. So interjection conversion is configured using a
configuration file. A default recommended configuration is provided. The configuration format is JSON with the following
fields:

| Field             | Default | Default     | Description                            |
|-------------------|---------|-------------|----------------------------------------|
| `segmentDuration` | 15000   | Integer     | Segment duration, in ms.               |
| `audio`           |         | Object list | Describes the audio streams to encode. |
| `video`           |         | Object list | Describes the video streams to encode. |


#### `video`

The default is to generate videos whichever of these videos are neither upscales nor duplicates:

 - Width 3840, source frame rate, and CRF 22.
 - Width 3840, `half+` frame rate, and CRF 22.
 - Width 1920, source frame rate, and CRF 22.
 - Width 1920, `half+` frame rate, and CRF 22.
 - Width 1280, `half+` frame rate, and CRF 25.
 - Width 640, `half+` frame rate, and CRF 28.

In each case, `h264` and `h265` variants are created.

| Field                     | Default | Type                               | Description                          |
|---------------------------|---------|------------------------------------|--------------------------------------|
| `width`                   |         | Integer                            | The width of the video stream.       |
| `height`                  |         | Integer                            | The height of the video stream.      |
| `frameRate`               |         | `[`Integer`,` Integer`]` or String | The frame rate to use for the video. |
| `crf`                     | 25      | Integer                            | Constant rate factor.                |
| `codec`                   | `h264`  | String                             | The video codec to use.              |


##### `video.width`

By default, the resolution of the input will be used. If only one of width and height is given, then the other will be
calculated from the aspect ratio of the input video.


##### `video.height`

By default, the resolution of the input will be used. If only one of width and height is given, then the other will be
calculated from the aspect ratio of the input video.


##### `video.frameRate`

By default, the frame rate of the input video is used. Use the special value of `half` to use half the frame rate of the
input video, or `half+` to halve the frame rate if the result would be at least 23 fps.


#### `audio`

The default is to generate bitrates of 192 kBit/s, 96 kBit/s, and 48 kBit/s.

| Field        | Default | Type    | Description             |
|--------------|---------|---------|-------------------------|
| `bitrate`    | 64      | Integer | The bitrate in kBit/s.  |
| `codec`      | `aac`   | String  | The audio codec to use. |


## `set-interjection-metadata`

The `set-interjection-metadata` program allows adding (or replacing) customer-defined metadata for an interjection.

In addition to metadata that describes the technical properties of an interjection, it's possible to set non-technical
metadata, such as metadata to describe what a particular interjection contains so that the client can choose between
interjections based on customer-defined criteria. The `set-interjection-metadata` program allows this metadata to be
attached to the other metadata for an interjection.


## `generate-interjection-set-metadata`

This tool generates metadata to describe a set of interjections.

Interjections for a channel are scheduled at the level of an interjection set. The interjection set describes the
available interjections that could be played that the client should choose between.

The `generate-interjection-set-metadata` generates a single file should be sent to the video streamer client.

Multiple interjection sets can co-exist in the same directory (and therefore share video files) by generating multiple
different interjection metadata files using `generate-interjection-set-metadata -n`.


## Metadata formats


### Interjection

The `interjection.json` file describes a single interjection. It has the following fields:

| Field             | Type    | Description                                                        |
|-------------------|---------|--------------------------------------------------------------------|
| `contentDuration` | Integer | The length of the interjection, in ms.                             |
| `segmentDuration` | Integer | The length of the interjection's segments, in ms.                  |
| `video`           | Array   | An array of all the video qualities the interjection provides.     |
| `audio`           | Array   | An array of all the audio qualities the interjection provides.     |
| `tag`             | Object  | A user-supplied object to be used to choose between interjections. |
| `other`           | Object  | A user-supplied object containing any other wanted metadata.       |


#### `video`

| Field       | Type            | Description                                                  |
|-------------|-----------------|--------------------------------------------------------------|
| `width`     | Integer         | The width of the video.                                      |
| `height`    | Integer         | The height of the video.                                     |
| `frameRate` | `[` Integer `]` | The frane rate of the video as a numerator/denominator pair. |
| `mimeType`  | Integer         | The mime-type of the video container.                        |
| `codecs`    | Integer         | The codec used for the video.                                |


#### `audio`

| Field       | Type    | Description                                                  |
|-------------|---------|--------------------------------------------------------------|
| `mimeType`  | Integer | The mime-type of the video container.                        |
| `codecs`    | Integer | The codec used for the video.                                |


### Interjection set

The interjection set metadata (`interjection-set.json` by default) describes the available interjections. The idea is
that the client can use this file to decide which interjection to show.

| Field           | Type   | Description                                                         |
|-----------------|--------|---------------------------------------------------------------------|
| `interjections` | Object | Map from interjection sub-path to metadata about that interjection. |
| `other`         | Object | A user-supplied object containing any other wanted metadata.        |


#### `interjections`

The `interjections` field is a map. The keys of the object are the names of the sub-directories (from the directory
containing the interjection set metadata file) for each interjection. The corresponding values describe the interjection
and have the following fields:

| Field      | Type    | Description                                       |
|------------|---------|---------------------------------------------------|
| `duration` | Integer | The length of the interjection, in ms.            |
| `tag`      | Object  | The `tag` field from the interjection's metadata. |
