# Stage 1: Build
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Add universe repo (needed for libfaac-dev)
RUN echo "deb http://archive.ubuntu.com/ubuntu noble universe" > /etc/apt/sources.list.d/noble-universe.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git protobuf-compiler ca-certificates \
    libssl-dev libsqlite3-dev libwxgtk3.2-dev libtag1-dev libmp3lame-dev \
    libogg-dev libvorbis-dev libsndfile1-dev libavcodec-extra libavformat-dev \
    libavcodec-dev libcurl4-openssl-dev libmpg123-dev libresample1-dev \
    libfaad-dev libncurses5-dev libphysfs-dev libpcre3-dev libprotobuf-dev \
    libmysqlclient-dev libfaac-dev libopus-dev libloudmouth1-dev \
    libdbus-glib-1-dev libmuparser-dev libsoxr-dev libz-dev \
    liblua5.4-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN mkdir build && cd build && cmake .. && make add_checksum && make -j"$(nproc)"

# Stage 2: Runtime
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Add Ubuntu universe repo (needed for libfaac0)
RUN echo "deb http://archive.ubuntu.com/ubuntu noble universe" > /etc/apt/sources.list.d/noble-universe.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3t64 libsqlite3-0 libtag1v5 libmp3lame0 libogg0 \
    libvorbis0a libvorbisenc2 libsndfile1 libavcodec60 libavformat60 \
    libcurl4t64 libmpg123-0 libresample1 libfaad2 \
    libphysfs-1.0-0 libpcre3 libprotobuf32t64 libmysqlclient21 \
    libopus0 libloudmouth1-0 libdbus-1-3 libmuparser2v5 \
    libsoxr0 liblua5.4-0 libfaac0 ca-certificates openssl \
    whiptail yt-dlp \
    && rm -rf /var/lib/apt/lists/*

# Run as a non-root user by default instead of root. UID/GID 1000 matches
# the first regular user on most Linux hosts; override at container-start
# time to match your actual host UID/GID (see docker-compose.yml's `user:`
# field) so files written to the bind-mounted ./data aren't owned by a
# mismatched UID.
# ubuntu:24.04 ships a default "ubuntu" user already on 1000:1000; drop it
# so we can reuse that UID/GID for our own user without a conflict.
RUN userdel -r ubuntu && \
    groupadd -g 1000 radiobot && \
    useradd -u 1000 -g radiobot -d /radiobot -M -s /usr/sbin/nologin radiobot

WORKDIR /radiobot

# Copy compiled binary and plugins from builder
COPY --from=builder /src/build/Output/radiobot ./
COPY --from=builder /src/build/Output/plugins/ ./plugins/
COPY docker-entrypoint.sh ./entrypoint.sh
COPY setup-tui.sh setup-plain.sh ./
COPY ircbot.text ./
RUN chmod +x entrypoint.sh setup-tui.sh setup-plain.sh

# tmp/ is where the bot copies plugin .so files before dlopen()ing them
RUN mkdir -p tmp data && chown -R radiobot:radiobot /radiobot

# AutoDJ (and Telnet, if enabled) look for a TLS cert/key at the hardcoded
# relative path "ircbot.pem", unrelated to the IRC TLS setting. A symlink
# into the persistent data volume lets the entrypoint generate a real,
# per-deployment cert at first run instead of baking one static private
# key into the image that every build of it would share.
RUN ln -s data/ircbot.pem ircbot.pem

# /radiobot/data is the persistent volume for ircbot.conf, database, logs, etc.
# Plugin paths in ircbot.conf should be relative, e.g.: plugins/AutoDJ.so
VOLUME ["/radiobot/data"]

USER radiobot

ENTRYPOINT ["./entrypoint.sh"]
