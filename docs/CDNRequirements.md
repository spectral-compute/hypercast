---
title: CDN Requirements for Low-latency mode
---

This livestreaming solution uses HTTP, but for optimal performance it is slightly picky about
some details of how the CDN behaves. This page aims to summarise those requirements in a generic way.

For step-by-step setup instructions for specific CDNs, see other parts of this manual.

## Correctly Handling Cache Misses

The most challenging requirement of this streaming solution on the CDN is a subtlety of how cache
misses are handled.

Scenario: multiple users simultaneously make an HTTP GET request for a static resource that is
not found in the CDN's cache.

Desired behaviour:

- A small constant number of CDN nodes - preferably one - should make the corresponding GET request
  to the origin.
- As data is received from the origin to satisfy this request, it should be immediately delivered
  to all waiting users, and cached (Optionally, caching can be done per-chunk of a chunked-transfer
  encoding request, but doing it at a finer granularity would be more efficient, and appears to be
  what CF does).
- If other users request the same file while this process is underway, the partial content should
  be available in the CDN's cache, and then the remainder is delivered as it arrives - as above.

This behaviour is highly desirable for all CDN HTTP workloads: it reduces
load on the origin server by avoiding a request storm in the event of a cache miss, and it
significantly improves page load times: a request storm will necessarily cause the origin to
satisfy each request slowly. When done properly, the origin has to satisfy only one request (which
means the lowest possible latency is attained), and its results are forwarded immediately to all
users over the CDN's network.

It's worth noting that this behaviour is also necessary for the proper operation of chunked HTTP
requests over the CDN. Some CDNs we have tested will never satisfy any request from their cache
until the entire request has been completed by the CDN. This is very problematic for applications
using chunked transfer encoding to deliver and process partial results.

## Respect origin cache TTL headers

Some CDNs forcibly overwrite all HTTP browser cache TTL headers with their own values. The correct
operation of the streaming application requires that the CDN propagate the specific values inserted
by the streaming server.

Of course, this requirement applies only to the routes that are actually backed by the streaming
server. Plenty of "normal" web content may benefit from TTL headers being given more aggressive values
by CDNs (which is presumably why the behaviour is so widespread).

## Tiered Caching (Desirable but not essential)

In the event of a cache miss, it is desirable for only one CDN node to request the resource from the
origin servers, and for other CDN nodes to obtain the data from the one that made the request (as
it occurs). This further reduces load on the origin server, which is desirable for all workloads,
and particularly impactful for relatively high bandwidth applications such as video. Some CDNs we
have tested instead make one request per CDN node, which multiplies the necessary bandwidth at the
video upload site by a constant factor.
