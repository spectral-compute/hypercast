#!/bin/bash

set -ETeuo pipefail

# Default arguments.
CPU=x86-64-v3
BUILD_ID="$(date -u +'%Y%m%d%H%M%S')"
BUILD_FFMPEG_ARGS=(-F decklink)

# Decode the arguments.
while [ "$#" != "0" ] ; do
    case "$1" in
        -h|--help)
            echo -e "Usage: build-ffmpeg.sh [-c cpu] [-f feature] [-F feature] [-n build_id]\n" \
                    " -c: The CPU to use. x86-64, x86-64-vX, native, or any other value accepted by clang's -march\n" \
                    "     and -mtune flags. The default is native.\n" \
                    " -f: Passed to build-ffmpeg.\n" \
                    " -F: Passed to build-ffmpeg.\n" \
                    " -n: The build ID to use (included in the Docker image tag).\n" \
                    "\n" \
                    "Builds a Docker image for RISE. The resulting binaries might not be redistributable under their\n" \
                    "standard licenses."
            exit 0
        ;;
        -c)
            CPU="$2"
            shift
        ;;
        -f)
            BUILD_FFMPEG_ARGS+=(-f "$2")
            shift
        ;;
        -F)
            BUILD_FFMPEG_ARGS+=(-F "$2")
            shift
        ;;
        -i)
            INSTALL="$(realpath "$2")"
            shift
        ;;
        *)
            echo "Error: Unknown argument: $1. Try $0 --help." 1>&2
            exit 1
        ;;
    esac
    shift
done

# Figure out what we're doing.
SRC="$(realpath "$(dirname "$0")"/..)"
VERSION="$(git -C "${SRC}" describe --always --dirty)"

echo "Building RISE docker container ${VERSION}-${BUILD_ID} for ${CPU} from ${SRC}"
echo "Arguments for build-ffmpeg: ${BUILD_FFMPEG_ARGS[@]}"

# Check the build tree doesn't exist.
for DIR in build install ; do
    if [ -e "${DIR}" ] ; then
        echo "${DIR} exists" !>&2
        exit 1
    fi
done

# Build the server.
mkdir -p build/server
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=build/server/install \
    -DENABLE_JS=Off \
    -DXCMAKE_TARGET_TRIBBLE=native-native-"${CPU}" \
    -DXCMAKE_CLANG_TIDY=On \
    -DXCMAKE_PACKAGING=Off \
    -DXCMAKE_ENABLE_DOCS=Off \
    -DXCMAKE_ENABLE_TESTS=Off \
    -Bbuild/server/build \
    "${SRC}"
make -C build/server/build install -j$(nproc)
rm build/server/install/bin/build-ffmpeg # Not needed by the Docker image.

# Build ffmpeg.
mkdir -p build/ffmpeg
"${SRC}/ffmpeg-tools/build-ffmpeg" \
    -b build/ffmpeg/build -i build/ffmpeg/install -s build/ffmpeg/source \
    -c "${CPU}" "${BUILD_FFMPEG_ARGS[@]}"
rm build/ffmpeg/install/bin/{x264,x265} # We don't need the CLI for the codecs.

# Build the docker image.
TAG="rise:${VERSION}-${BUILD_ID}-${CPU}"
TAR="install/rise-docker-${VERSION}-${BUILD_ID}-${CPU}.tar"

mkdir install
mkdir -p build/docker

cp -r "${SRC}/docker/image" build/docker/src
cp -r build/server/install build/docker/src/server
cp -r build/ffmpeg/install build/docker/src/ffmpeg

docker pull archlinux
docker build --no-cache -t "${TAG}" build/docker/src
docker save "${TAG}" > "${TAR}"
docker image rm "${TAG}"

# Compress the above images.
gzip -9k "${TAR}" &
xz -9k "${TAR}" &
wait
