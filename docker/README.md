# RISE Docker container

 - [./RiseDockerContainer.md](User guide)

## Building

Basic usage is as follows:
 - Usage: `mkDockerImage.sh [-c cpu] [-f feature] [-F feature] [-n build_id]`
 - The `-c`, `-f`, and `-F` options are as accepted by the `build-ffmpeg` tool.
 - Unlike `build-ffmpeg`, `mkDockerImage.sh` does not build the Decklink support by default, as the Docker image does
   not yet support it.
 - The `-n` option specifies a build ID/name. If not given, a timestamp is used.

The script:
 - Can be run out-of-tree.
 - Creates `build` and `install` in the current working directory. These must not already exist.
 - Builds the RISE server and `ffmpeg`.
 - Generates a tarball (and compressed versions of it) in `install` that can be imported with `docker load` for the RISE
   Docker image.

### Example

`./mkDockerImage.sh`
