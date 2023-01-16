#!/bin/bash

set -e

# The source directory.
SRC="$(realpath "$(dirname "$0")")"

# Get into a directory to play with. We need this because so we don't mess up the source repository. Stupid lack of
# out-of-tree builds.
rm -rf /tmp/lvss-package
rsync -a "${SRC}/" /tmp/lvss-package
cd /tmp/lvss-package
ls

# Clean up.
yarn clean
rm -rf spectral-video-streamer-*.tar.gz

# Yarn has the stupid property that it needs a plugin to do a production install. Install that badly named plugin here
# rather than in git because doing the latter would mean committing the plugin itself into git - yuck!
yarn plugin import workspace-tools
yarn workspaces focus --production --all

# Install other dependencies.
yarn install

# Build
yarn bundle
yarn foreach lint
yarn docs

# Test.
yarn test

# Actually generate the package.
bash packageArchive.sh
EXIT=$?

# Copy the package and be done.
if [ "${EXIT}" == "0" ] ; then
    cp spectral-video-streamer-*.tar.gz "${SRC}"
    exit
fi
exit ${EXIT}
