#!/bin/bash

set -ETeuo pipefail

# Check that the configuration directory exists.
if [ ! -e "/config" ] ; then
    ls /
    echo "Error: /config not mounted!" 1>&2
    echo "       Add --mount type=bind,src=YOUR_CONFIG_DIRECTORY,dst=/config to your docker command." 1>&2
    exit 1
fi

# Check that there's a configuration file in it.
if [ ! -e "/config/lvss-config.json" ] ; then
    echo "Error: /config/lvss-config.json does not exist. Creating from an example. Please adjust it as needed." 1>&2
    sudo -u server cp /opt/rise/example-lvss-config.json /config/lvss-config.json
    exit 1
fi

# Log directory.
if [ -e "/log" ] ; then
    DATE="$(date +'%Y-%m-%d_%H-%M-%S')"
    LOG="/log/${DATE}"
    mkdir -p "${LOG}"
    rm -f /log/latest
    ln -s "${DATE}" /log/latest
fi

# Function to either log, or not, a command
function run
{
    NAME="$1"
    DESCRIPTION="$2"
    shift 2

    if [ -e "/log" ] ; then
        echo "${DESCRIPTION} with logging to ${LOG}/${NAME}.log"
        "$@" 2>&1 | tee "${LOG}/${NAME}.log"
    else
        echo "${DESCRIPTION}"
        "$@"
    fi
}

# If there's a Cloudflared token, run Cloudflared.
if [ -e "/config/cloudflared.token" ] ; then
    run "cloudflared" "Connecting to Cloudflare Tunnel" \
        cloudflared --no-autoupdate tunnel run --token "$(cat "/config/cloudflared.token")" &
fi

# Run the server.
PATH="/opt/rise/server/bin:/opt/rise/ffmpeg/bin:$PATH"
run "lvss" "Running RISE server" sudo -u server live-video-streamer-server /config/lvss-config.json
