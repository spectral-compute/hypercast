#!/bin/bash

set -e

# Argument parsing.
SRCDIR="$(realpath "$(dirname "$0")")"
OUTDIR="$(realpath $1)"
shift

# Create temporary directory and copy the markdown to it.
TMP=/tmp/live-video-streamer-docs
rm -rf "${TMP}"
mkdir "${TMP}"
cp -r ${SRCDIR} "${TMP}/markdown"

# Function for using typedoc to build markdown files for the documentation.
function typedocify {
    WORKSPACE="$1"
    SOURCE="$2"
    shift 2

    echo -e "Processing \e[1m${WORKSPACE}/${SOURCE}\e[m with TypeDoc\e[m"

    # Run typedoc.
    rm -rf "${TMP}/typedoc"
    yarn typedoc --tsconfig "../${WORKSPACE}/tsconfig.json" --out "${TMP}/typedoc" "../${WORKSPACE}/src/${SOURCE}.ts"

    # Append to the templates.
    while [ "$#" != "0" ] ; do
        SRC="$1"
        DST="$2"
        TITLE="$3"
        SUBTITLE="$4"
        shift 4

        mkdir -p "$(dirname "${TMP}/markdown/${DST}")"
        echo -e "---\ntitle: ${TITLE}\ntoc: false\n---" > "${TMP}/markdown/${DST}.md"
        if [ ! -z "${SUBTITLE}" ] ; then
            echo -e "\n# ${SUBTITLE}" >> "${TMP}/markdown/${DST}.md"
        fi
        tail -n +4 "${TMP}/typedoc/${SRC}.md" >> "${TMP}/markdown/${DST}.md"
    done
}

# Function for substituting in markdown.
function substitute {
    MD="$1"
    TOKEN="$2"
    REPLACEMENT="$3"
    RESULT="$(echo -n "${REPLACEMENT}" |
              python3 -c "import sys; print(open('${MD}').read().replace('{${TOKEN}}', sys.stdin.read()))")"
    echo -n "${RESULT}" > "${MD}"
}

# Change to the documentation source directory.
cd "${SRCDIR}"

# Run typedoc to build reference documentation.
typedocify client Main classes/Player ClientAPI "Ultra low-latency video streamer client API" "\`Player\` class"
typedocify server Config/Spec \
           README ServerConfigSpec/README "Ultra low-latency video streamer server configuration" "" \
           interfaces/AudioConfig ServerConfigSpec/interfaces/AudioConfig \
               "Ultra low-latency video streamer server configuration" "\`AudioConfig\` interface" \
           interfaces/BufferControl ServerConfigSpec/interfaces/BufferControl \
               "Ultra low-latency video streamer server configuration" "\`BufferControl\` interface" \
           interfaces/FilesystemDirectoryConfig ServerConfigSpec/interfaces/FilesystemDirectoryConfig \
               "Ultra low-latency video streamer server configuration" "\`FilesystemDirectoryConfig\` interface" \
           interfaces/CodecOptions ServerConfigSpec/interfaces/CodecOptions \
               "Ultra low-latency video streamer server configuration" "\`CodecOptions\` interface" \
           interfaces/Config ServerConfigSpec/interfaces/Config \
               "Ultra low-latency video streamer server configuration" "\`Config\` interface" \
           interfaces/SourceConfig ServerConfigSpec/interfaces/SourceConfig \
               "Ultra low-latency video streamer server configuration" "\`SourceConfig\` interface" \
           interfaces/VideoConfig ServerConfigSpec/interfaces/VideoConfig \
               "Ultra low-latency video streamer server configuration" "\`VideoConfig\` interface"
echo -e "Finished running \e[1mTypeDoc\e[m"

# Patch the defaults into the ServerConfiguration documentation.
substitute "${TMP}/markdown/ServerConfiguration.md" "DEFAULTS" \
           "$(echo "$(sed -E 's/export//' "../server/src/Config/Default.ts")
                    console.log(JSON.stringify(defaultConfig, null, 2));" | node)"

# Build the documentation with pandoc.
echo -e "Running \e[1mpandoc\e[m"
"xcmake/tools/pandoc/standalone.sh" "${TMP}/markdown" "${OUTDIR}" live-video-streamer -- "$@"

# Clean up.
rm -rf "${TMP}"
