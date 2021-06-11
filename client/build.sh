#!/bin/bash

OUT="$1"

DIR="$(dirname "$0")"

tsc --outDir "${OUT}"
cp "${DIR}/liveStreamExample.html" "${OUT}"
