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
2. Set [Browser Cache TTL](https://developers.cloudflare.com/cache/about/edge-browser-cache-ttl#browser-cache-ttl) to
   "Respect Existing Headers".
    1. Go to "Caching" → "Configuration" in the left sidebar.
    2. Under "Browser Cache TTL", choose "Respect Existing Headers".
3. Enable [Tiered Cache](https://developers.cloudflare.com/cache/how-to/enable-tiered-cache/).
    1. Go to "Caching" → "Tiered Cache" in the left sidebar.
    2. Under "Argo Tiered Cache", turn the slider to the right (on).
    3. If offered "Tiered Cache Topology", choose "Smart Tiered Cache Topology".


## Explanation

This section discusses the domain settings and [Page Rule](https://support.cloudflare.com/hc/en-us/articles/218411427)
settings that that should be used for the ultra low-latency video streamer in more detail.


### Always Online

None of the ultra low-latency video streamer's resources can usefully be cached for long periods of time. In any case,
the client relies on up-to-date information from the server. Thus any data that makes it into the
[Always Online](https://developers.cloudflare.com/cache/about/always-online) cache is not useful. Additionally, this
feature could result in stale data being served if the origin server is offline, which could cause confusion but would
never be helpful. 


### Browser Cache TTL

Cloudflare changes the cache control headers sent by an origin server when sending them to the client if they specify 
aching for less time than the
[Browser Cache TTL](https://developers.cloudflare.com/cache/about/edge-browser-cache-ttl#browser-cache-ttl) setting
specifies. The "Respect Existing Headers" causes Cloudflare not to do this.

The "Respect Existing Headers" option is not available as a Page Rule.


### Tiered Cache

Use of [Tiered Cache](https://developers.cloudflare.com/cache/how-to/enable-tiered-cache/) is recommended to minimize
traffic at the origin server. We have also found that in practice, Cloudflare's internal international infrastructure
can give better performance than accessing the origin server by a distant edge server.

Note that this does *not* require enabling "Argo Smart Routing", for which there is an extra charge.

Cloudflare offer [different tier topologies](https://support.cloudflare.com/hc/en-us/articles/115000224552) for
Enterprise plans. The default (and also only one available on plans without the option) is "Smart Tiered Cache
Topology". This makes Cloudflare connect to the streamer origin server using a nearby edge server, and transmits from
there to the edge server that is serving the client's request. In most cases, this the preferred option and should give
the improved performance discussed above.

This setting is not available as a Page Rule.


### Cache Level

By default, Cloudflare caches only some resources, chosen
[based on file extension](https://developers.cloudflare.com/cache/about/default-cache-behavior/#default-cached-file-extensions).
The ultra low-latency video streaming server requires a high degree of control over what is
cached, and so transmits appropriate cache control headers. Setting to
"[Cache Everything](https://developers.cloudflare.com/cache/how-to/create-page-rules/#cache-everything)" causes
Cloudflare to rely on those headers.

The "Cache Everything" option is not available globally.


### Response buffering

Cloudflare offers [Response Buffering](https://support.cloudflare.com/hc/en-us/articles/206049798) for Enterprise plans.
When this is turned on, Cloudflare waits for a full file to be received by Cloudflare before sending it to clients. This
is not compatible with low latency streaming, and so must be disabled.

It is disabled by default, but there is a global setting to enable it. Adding the Page Rule (even if the global setting
is disabled) ensures that if the global setting is changed, streaming will not be broken.
