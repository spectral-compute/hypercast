# Figure out what the package should be called.
PACKAGE="spectral-video-streamer-$(git describe --all --long --dirty | sed -E 's,^(heads|tags)/,,;s,/,_,g').tar.gz"

# Remove the existing package if it exists.
rm -f "${PACKAGE}"

# Make a new package.
tar c \
    --exclude 'asset-manifest.json' \
    --transform 's,^,spectral-video-streamer/,' \
    --transform 's,/build/,/,;s,/build$,,' \
    --transform 's,/dist/,/,;s,/dist$,,' \
    --transform 's,/docs/live-video-streamer/,/,;s,/docs/live-video-streamer,,' \
    client/dist demo-client/build server/dist docs/dist/docs/live-video-streamer \
    | gzip -9 > "${PACKAGE}"

# Check the package contents.
EXIT=0
for F in $(tar tf "${PACKAGE}") ; do
    # Make sure it's in the correct directory.
    if ! echo "${F}" | grep -qE '^spectral-video-streamer/' ; then
        echo -e "\x1b[31;1mError\x1b[m: Package entry ${F} is a tar bomb file." 2>&1
        EXIT=1
    fi

    # Make sure we didn't install anything private.
    if echo "${F}" | grep -qF 'private' ; then
        echo -e "\x1b[31;1mError\x1b[m: Package entry ${F} is private." 2>&1
        EXIT=1
    fi

    # Make sure it's a file we're expecting.
    if echo "${F}" | grep -qE '/$' ; then
        continue
    fi
    if echo "${F}" | grep -qE '\.(css|html|js|LICENSE\.txt|d\.ts)$' ; then
        continue
    fi
    echo -e "\x1b[31;1mError\x1b[m: Package entry ${F} of unexpected suffix." 2>&1
    EXIT=1
done

# Print the contents so we can see it.
echo -e "\x1b[32;1mCreated package\x1b[m: ${PACKAGE}:"
tar tf "${PACKAGE}"
echo $(expr $(stat -c '%s' spectral-video-streamer-feat_stuff2023011601-0-g891c54b-dirty.tar.gz) / 1024) kiB

# Print a warning, because this package is not "unfriendly? customer" friendly.
echo -e "\x1b[33;1mWARNING\x1b[m: This script made a package in a draft format!" 2>&1
echo "         E.g: The license files are confusing." 2>&1

# Done :)
exit ${EXIT}
