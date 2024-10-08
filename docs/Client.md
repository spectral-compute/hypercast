---
title: Ultra low-latency video streamer client
toc: true
---

Spectral Compute's ultra low-latency streamer is designed to allow video streaming over a Content Delivery Network (CDN)
with latencies as low as one second. Thus you can benefit from the scalability of a CDN while still achieving low
latency streaming.


## Client

The client is a library to be included in your application. It connects to the CDN, which in turn connects to the
server. The client uses the standard
[Media Source Extensions](https://developer.mozilla.org/en-US/docs/Web/API/Media_Source_Extensions_API) provided by
modern web browsers for efficient video decode and display in a standard plugin-free way.

 - [Client API](./ClientAPI.md) reference


## Demo Client

There is a demo client written in [Typescript](https://www.typescriptlang.org/) to demonstrate use of the ultra
low-latency streamer client in an application.
