---
title: Low latency techniques
toc: true
---


## Pre-availability

Traditional DASH implementations rely on clock synchronization to determine when a segment will become available. Clocks
are typically synchronized to within a couple of seconds using an HTTP-based timing URL, such as
[Akamai's](https://time.akamai.com/?iso). The client should not request a segment early, or the server will respond with
an HTTP `404 Not Found` status code. To avoid this, clients typically wait a few seconds past the advertised release
time before attempting to fetch a segment. A client could be designed to poll the server to find out when a segment is
released, but the latency scales inversely proportional to the poll rate. Additionally, the process of establishing
connections and exchanging headers  between multiple machines in the network adds to the latency.

To solve these problems, the ultra low-latency video streamer uses a concept called "pre-availability". Instead of
responding to a segment that will be released "soon" (i.e: within a few seconds) with a `404 Not Found` status code, the
server behaves as if the segment is already avaiable, but doesn't actually serve the first byte until it becomes
available. This means the connections are already established, and headers exchanged. Since a connection is already
open, the client can be notified of the availability of a new segment just by transmitting its start.


### Synchronization

Traditional DASH clock synchronization is not robust when sub-second accuracy is required. Although the above scheme
can allow for synchronization on larger time scales (e.g: 2 seconds), I found that this is not the best approach due to
clock drift, awkward semantics for calculating segment index, and the accuracy needed.

Instead, the server publishes up-to-date information about what the latest segment is, and when it was published. This
automatically corrects for clock drift, and a maximum discrepency between the client's estimate of a segment's
pre-availability and the server's actual decision of pre-availability can be imposed by HTTP cache control (which
typically has 1 second granularity). This information is very small, and is intended to be cached by the CDN for 1
second.


## Interleaving

Cloudflare's network includes a 32 kiB buffer that can't be disabled even when the edge server starts serving responses
before it has received the complete response from the origin server. For video, this is not an unmanageable issue as at
1 Mib/s, a 32 kiB buffer corresponds to 250 ms. For audio, however, a 32 kiB buffer could easily be several seconds
long.

To prevent the audio from getting stuck in the buffer, the audio and video are interleaved together so that the video
evicts the audio from the buffer.

### Format

The format is very simple.

Each original file has a corresponding index in the file into which it is interleaved. The contents of the source files
are inserted into the interleaved file in chunks. The format is simply a repetition of:

 - An 8-bit content ID.
    - The low 5 bits are the file index.
    - Bit 5 indicates whether there's a timestamp included for this chunk.
    - The high 2 bits are the width of the length:
      - 0: 8-bit length.
      - 1: 16-bit length.
      - 2: 32-bit length.
      - 3: 64-bit length.
 - The N-bit little-endian length.
 - The timestamp, if present. This is a 64-bit unsigned integer in µs.
 - The data chunk.

The end of an original file is signalled by a zero-byte chunk for the given file index. File index 31 (i.e: the index
with all 1s) is reserved for padding.
