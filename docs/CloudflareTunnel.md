---
title: Configuring Cloudflare Tunnel
toc: true
---

This page explains how to set up [Cloudflare Tunnel](https://www.cloudflare.com/en-gb/products/tunnel/) to access a RISE
server without port forwarding. Note that Cloudflare's services change from time to time, so this information is correct
at the time of writing.

## Get to the tunnel configuration panel

1. Log into Cloudflare.
2. Go to your organization.
3. Go to Zero Trust.
4. Go to Network → Tunnels.


## Create a tunnel

### Create the Cloudflare end of the tunnel

1. Click "Create a tunnel"
2. When prompted to choose a tunnel type, leave the default at "Cloudflared".
3. Enter a tunnel name.
4. Click Save tunnel.


### Connect to the tunnel

You should now be on the "Install and run a connector" page.

1. `sudo pacman -S cloudflared`
2. Install the service in `cloudflared` with the tunnel's token: `sudo cloudflared service install "${TOKEN}"`. You can
   get this command with the full token by clicking on the command below "If you already have cloudflared installed on
   your machine" when setting up the tunnel.
3. `sudo systemctl enable cloudflared`
4. `sudo systemctl start cloudflared`


### Set up SSH

You should now be on the "Route Traffic for" page.

1. Enter the subdomain you want, and your domain.
2. Add SSH and `localhost:22`.


### Connect to SSH

You should now be back on the "Tunnels" page, and the new tunnel should have healthy status.

1. `sudo pacman -S cloudflared`
2. Add the following to `~/.ssh/config`:
```
Host SUBDOMAIN.DOMAIN
    ProxyCommand /usr/bin/cloudflared access ssh --hostname %h
```


### Create another forwarding

1. Click the tunnel, and choose configure.
2. Go to "Public Hostname".
3. Click "Add a public hostname".
4. Fill in details, as in "Set up SSH", except choose the desired type and URL.
