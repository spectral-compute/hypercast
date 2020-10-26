#!/bin/bash

OUT="$1"

DIR="$(dirname "$0")"

tsc -t ES2017 --outDir "${OUT}" $(find "${DIR}" -type f -path '*.ts')
cp "${DIR}/liveStreamExample.html" "${OUT}"
