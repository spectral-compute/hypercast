# We use ArchLinux.
FROM archlinux

# Copy initial configuration and other files.
COPY etc /etc
COPY opt /opt

# Set up basic things.
ENV LANG en_GB.UTF-8
ENV LC_ALL en_GB.UTF-8
RUN locale-gen

# Sometimes, the keyring needs to be updated before anything else.
RUN pacman --noconfirm -Syy archlinux-keyring

# Update and install packages we either need or that are nice for sanity.
RUN pacman --noconfirm -Su boost-libs cloudflared fontconfig freetype2 numactl opus sudo ttf-dejavu zeromq

# Add the server user.
RUN useradd -m server

# Copy the binaries.
COPY server /opt/rise/server
COPY ffmpeg /opt/rise/ffmpeg

# Use pacman to delete unneeded bits of packages.
RUN rm $(pacman -Ql ttf-dejavu | cut -d ' ' -f 2 | grep -vE 'licenses|/DejaVuSansMono|(/$)')

# Delete packages we don't need.
RUN pacman --noconfirm -R archlinux-keyring base argon2 cryptsetup device-mapper gzip hwdata kbd kmod libutempter \
                          pciutils popt procps-ng psmisc systemd systemd-sysvcompat tar

# Delete pacman.
RUN yes | pacman -Scc
RUN pacman --noconfirm -R gawk gettext gnupg gpgme json-c libarchive libassuan libksba libsecret libusb libxml2 mpfr \
                          npth pacman pacman-mirrorlist pinentry tpm2-tss
RUN rm -rf /etc/pacman.d /var/lib/pacman/sync/*.db

# Delete headers.
RUN rm -rf /usr/include

# Delete static libraries.
RUN find /usr/lib -name '*.a' -exec rm {} +

# Remove programs we don't need. There are more, but we need to explicitly list them to delete them.
RUN rm -f /usr/bin/{captree,showdb,showstat4,sqldiff,sqlite3}

# Delete libraries we don't need.
RUN rm -f /usr/lib/libasan.so* /usr/lib/liblsan.so* /usr/lib/libtsan.so* /usr/lib/libubsan.so* \
          /usr/lib/libgfortran.so* /usr/lib/libgomp.so* \
          /usr/lib/libgdruntime.so* /usr/lib/libgphobos.so* /usr/lib/libgo.so*

# Delete documentation.
RUN rm -rf /usr/share/doc /usr/share/help /usr/share/info /usr/share/man

# Delete CMake
RUN rm -rf /usr/lib/cmake

# Delete Python.
RUN rm -rf /usr/lib/python*
RUN find / -name __pycache__

# Delete locales we don't need.
RUN find /usr/share/locale -mindepth 1 -maxdepth 1 \
         ! -name en ! -name en_GB ! -name en_US ! -name locale.alias -exec rm -rf {} +
RUN find /usr/share/i18n/locales -mindepth 1 -maxdepth 1 ! -name en ! -name en_GB ! -name en_US -exec rm -rf {} +


# Squash the above commands.
FROM scratch
COPY --from=0 / /

# Run the script to check the user-run Docker container on startup.
ENTRYPOINT ["bash", "/opt/rise/entrypoint.sh"]
