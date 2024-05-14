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
RUN pacman --noconfirm -Su boost-libs fontconfig freetype2 numactl opus ttf-dejavu zeromq nano

# Add the server user.
RUN useradd -m server

# Copy the binaries.
COPY server /opt/rise/server
COPY ffmpeg /opt/rise/ffmpeg

# Delete useless things.
RUN yes | pacman -Scc
RUN rm -f /var/lib/pacman/sync/*.db

RUN rm -rf /usr/include /usr/lib/cmake /usr/lib/python* \
           /usr/share/doc /usr/share/help /usr/share/info /usr/share/man \
           /usr/lib/libasan.so* /usr/lib/liblsan.so* /usr/lib/libtsan.so* /usr/lib/libubsan.so* \
           /usr/lib/libgphobos.so* /usr/lib/libgo.so*

RUN find /usr/lib -name '*.a' -exec rm {} +
RUN find /usr/share/locale -mindepth 1 -maxdepth 1 \
         ! -name en ! -name en_GB ! -name en_US ! -name locale.alias -exec rm -rf {} +
RUN find /usr/share/i18n/locales -mindepth 1 -maxdepth 1 ! -name en ! -name en_GB ! -name en_US -exec rm -rf {} +
RUN find / -name __pycache__

RUN rm $(pacman -Ql ttf-dejavu | cut -d ' ' -f 2 | grep -vE 'licenses|/DejaVuSansMono|(/$)')

# Squash the above commands.
FROM scratch
COPY --from=0 / /

# Run as server user.
USER server

# Run the script to check the user-run Docker container on startup.
ENTRYPOINT ["bash", "/opt/rise/entrypoint.sh"]
