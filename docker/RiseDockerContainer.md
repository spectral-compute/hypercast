# RISE Docker container user guide

## Getting started

The Docker image for [RISE](https://spectralcompute.co.uk/rise) runs a RISE video streaming server. It listens on port
8080 for the CDN. It expects to receive a configuration directory mounted to `/config`. Optionally, it can record a log
if `/log` is mounted.

### Example

1. Create the configuration and log directories:
```bash
mkdir config
mkdir log
```
2. [Load](https://docs.docker.com/reference/cli/docker/image/load/) the Docker image for RISE.
```bash
docker image load < rise-docker-0123456-demo-x86-64-v3.tar.xz
```
3. Generate an example configuration in `config/lvss-config.json`.
```bash
docker run --rm --mount type=bind,src=config,dst=/config rise:0123456-demo-x86-64-v3
```
4. [Run](https://docs.docker.com/reference/cli/docker/container/run/) the RISE docker image in a container. The default
   configuration generated in step 3 listens for an RTMP connection on port 1935.
```
docker run -d --rm \
    --mount type=bind,src=config,dst=/config \
    --mount type=bind,src=log,dst=/log \
    -p 1935:1935 -p 8080:8080 rise:0123456-demo-x86-64-v3
```
5. Stream a video to port 1935 on the Docker container's host.
