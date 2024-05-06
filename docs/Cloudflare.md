---
title: Configuring Cloudflare for the ultra low-latency video streamer
toc: true
---

This page explains how to set up Cloudflare to work with the ultra low-latency video streamer using the Cloudflare UI.
Note that Cloudflare's services change from time to time, so this information is correct at the time of writing.

## Quick setup

1. Log in and go to the dashboard for your domain.
2. [Add a subdomain](https://developers.cloudflare.com/dns/manage-dns-records/how-to/create-dns-records/#using-the-dashboard)
   for your streamer origin server. This guide will use the example: `stream.example.com`.
   1. Go to "DNS" in the sidebar on the left.
   2. Click "Add record" under "DNS management for example.com".
   3. Set the type to `A` if your origin server uses IPv4, or `AAAA` if it uses IPv6. Cloudflare provides IPv4 and IPv6
      access to clients in either case.
   4. Enter a subdomain name (`stream` in our example).
   5. Enter the IP address of your origin server.
   6. Ensure the "Proxy status" is set to on (the default).
   7. Click "Save".
3. Create a [Page Rule](https://support.cloudflare.com/hc/en-us/articles/218411427) for the streamer.
   1. Go to "Rules" in the left sidebar.
   2. Set the URL to include all resources on the subdomain, for example: `stream.example.com/*`.
   3. If available (it's already disabled if not), disable
      [Response Buffering](https://support.cloudflare.com/hc/en-us/articles/206049798).
      1. Under "Pick a Setting", choose "Response Buffering". If this option does not exist, it's probably not available
         for your plan.
      2. Ensure the setting is set to off (the default).
      3. Click "Add a setting"
   4. Disable [Always Online](https://developers.cloudflare.com/cache/about/always-online).
      1. Under "Pick a Setting", choose "Always Online".
      2. Ensure the setting is set to off (the default).
      3. Click "Add a setting"
   5. Set the cache level to
      "[Cache Everything](https://developers.cloudflare.com/cache/how-to/create-page-rules/#cache-everything)".
      1. Under "Pick a Setting", choose "Cache Level".
      2. Under "Select Cache Level", choose "Cache Everything".
   6. Add any other Page Rule settings you need (only the first matching Page Rule will apply, so this rule will need
      to contain any other settings you require).
   7. Under "Order", choose "First".
   8. Click "Save and Deploy Page Rule".
4. Set [Browser Cache TTL](https://developers.cloudflare.com/cache/about/edge-browser-cache-ttl#browser-cache-ttl) to
   "Respect Existing Headers".
    1. Go to "Caching" → "Configuration" in the left sidebar.
    2. Under "Browser Cache TTL", choose "Respect Existing Headers".
5. Enable [Tiered Cache](https://developers.cloudflare.com/cache/how-to/enable-tiered-cache/).
    1. Go to "Caching" → "Tiered Cache" in the left sidebar.
    2. Under "Argo Tiered Cache", turn the slider to the right (on).
    3. If offered "Tiered Cache Topology", choose "Smart Tiered Cache Topology".


## Explanation

This section discusses the domain settings and [Page Rule](https://support.cloudflare.com/hc/en-us/articles/218411427)
settings that that should be used for the ultra low-latency video streamer in more detail.

The settings desired by the streaming software need only be applied to routes that map to the RISE streaming server.
This means that - where possible - page rules may be used to apply the settings selectively, and you can use alternative
settings for the remainder of your site. An alternative solution is to serve all low-latency streaming content through
a dedicated domain that has the necessary CDN settings (a configuration that may be less accident-prone than maintaining
cloudflare page rules).

### Always Online

["Always Online"](https://developers.cloudflare.com/cache/about/always-online) is a feature that serves the last-known "successful" version of your website to the user when the origin
server returns an error code. For low-latency streaming, all this actually does is serves the tail end of the last
stream to users long after the stream has stopped broadcasting. Obviously, this should be disabled.

### Browser Cache TTL

Cache TTL headers tell browsers how long to cache something for. The correct operation of the streaming serevr requires
setting these in a very particular way. Unless told otherwise, Cloudflare will modify the cache control headers if they
specify caching for less time than the
[Browser Cache TTL](https://developers.cloudflare.com/cache/about/edge-browser-cache-ttl#browser-cache-ttl) setting
specifies.

"Respect Existing Headers" causes Cloudflare not to do this, which allows our server software to set the
headers as needed.

### Tiered Cache

It's important that the CDN only request each video segment once from the origin server. Otherwise, it is easy to
saturate the available uplink of the origin server, or to take it offline if a surge of new viewers becomes a surge of
requests to the origin server.

Cloudflare is generally good about this, but occasionally multiple Cloudflare nodes will make separate requests to
the origin server. The [Tiered Cache](https://developers.cloudflare.com/cache/how-to/enable-tiered-cache/)
feature can be used to guarantee that only one CDN node ever makes requests to the origin, and all other CDN nodes
get their information from that first one. To achieve this, you generally want to pick "Smart Tiered Cache Topology",
which is the default behaviour (and the only available one for non-Enterprise plans).

### Argo Smart Routing

"Argo" is a feature that uses Cloudflare's internal network to send data long geographical distances, dynamically
routing around congestion on the internet. We find that this has positive effects on latency and reliability (especially
when there is a large geographical distance between video source and viewer), but it incurs extra fees from Cloudflare.

### Cache Level

By default, Cloudflare caches only some resources, chosen
[based on file extension](https://developers.cloudflare.com/cache/about/default-cache-behavior/#default-cached-file-extensions).
The ultra low-latency video streaming server requires a high degree of control over what is
cached, and so transmits appropriate cache control headers. Setting to
"[Cache Everything](https://developers.cloudflare.com/cache/how-to/create-page-rules/#cache-everything)" causes
Cloudflare to rely on those headers.

This setting almost certainly should be a page rule (or sandboxed via a separate domain), since "cache everything" may
break other web services by causing stale results to be sent to the user.

### Response buffering

[Response Buffering](https://support.cloudflare.com/hc/en-us/articles/206049798) causes Cloudflare to wait for a full
file to be received by Cloudflare before sending it to clients. This is not compatible with low latency streaming, an
so must be disabled.
