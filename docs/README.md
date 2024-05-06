---
title: Ultra low-latency video streamer
toc: true
---

Spectral Compute's ultra low-latency streamer is designed to allow video streaming over a Content Delivery Network (CDN)
with sub-second latency. This allows you to benefit from the scalability and economies of simple HTTP caching CDNs while
still achieving ultra low-latency streaming.

The solution consists of:

- A server that ingests video, transcodes it appropriately, and delivers it to the CDN.
- Various client-side libraries (players) for displaying the stream.

## Client

The nature of the video stream is quite similar to MPEG-DASH, but the actual format that is transmitted over the wire
is rather different to achieve some of the performance benefits of our solution. There is also some unique buffer
management logic present in the client that is responsible for some of the latency savings.

Currently, we have implementations of the frontned library for three popular web frontend technologies:

- Vanilla JS
- React
- Angular

Support for more platforms/frameworks is available on demand.

## Server

The server ingests video streams in any format supported by ffmpeg, uses a slightly tweaked ffmpeg to transcode them
to an appropriately-configured MPEG-DASH stream, and performs the necessary packaging to deliver the video at low
latency over the network. The featureset of supported DASH features is approximately the same as that of ffmpeg - notably
including variants, subtitle streams, and multiple channels.

The current solution uses software encoding to ensure the best possible bitrates and quality. Hardware encoders would
also be supported, but a modest amount of integration work may be required.

This server functions as the origin from the point of view of the CDN. It can be onsite, physically close to the actual
video source. It can be elsewhere, though that would necessitate uploading the video to it before it has been transcoded
into a broadcast-friendly state. This might place an undue burden on the available upload capacity at the location of
the video source. Which option is best depends on the constraints at the site of the video source.

## Content Distribution Network

Your CDN needs to be configured appropriately for the ultra low-latency video streamer to work properly. 

 - [CDN Requirements](./CDNRequirements.md) discusses the necessary configuration for any HTTP CDN.
 - [Cloudflare](./Cloudflare.md): Step-by-step guide for setting up with Cloudflare.
