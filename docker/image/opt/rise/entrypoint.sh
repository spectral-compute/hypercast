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
    cp /opt/rise/example-lvss-config.json /config/lvss-config.json
    exit 1
fi

# Run the server.
echo "Starting the RISE server..."
PATH="/opt/rise/server/bin:/opt/rise/ffmpeg/bin:$PATH"

if [ -e "/log" ] ; then
    live-video-streamer-server /config/lvss-config.json 2>&1 | tee "/log/$(date +'%Y%m%d%H%M%S.log')"
else
    live-video-streamer-server /config/lvss-config.json
fi
