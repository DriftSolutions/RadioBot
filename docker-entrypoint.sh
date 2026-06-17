#!/bin/bash
set -e

CONFIG="/radiobot/data/ircbot.conf"
TEXTFILE="/radiobot/data/ircbot.text"

mkdir -p /radiobot/data

# Seed default ircbot.text into volume on first run so the user can edit it
if [ ! -f "$TEXTFILE" ]; then
    cp /radiobot/ircbot.text "$TEXTFILE"
fi

# AutoDJ/Telnet look for a TLS cert/key at the relative path "ircbot.pem"
# (symlinked to data/ircbot.pem in the image, see Dockerfile). Generate a
# per-deployment self-signed cert here on first run instead of shipping a
# static one baked into the image.
PEMFILE="/radiobot/data/ircbot.pem"
if [ ! -f "$PEMFILE" ]; then
    openssl req -x509 -newkey rsa:2048 -keyout /tmp/key.pem -out /tmp/cert.pem \
        -days 3650 -nodes -subj "/CN=radiobot" 2>/dev/null
    cat /tmp/key.pem /tmp/cert.pem > "$PEMFILE"
    rm -f /tmp/key.pem /tmp/cert.pem
fi

if [ -f "$CONFIG" ]; then
    exec ./radiobot --conf="$CONFIG" --text="$TEXTFILE"
fi

if [ -t 0 ] && [ -t 1 ]; then
    if command -v whiptail >/dev/null 2>&1; then
        exec ./setup-tui.sh
    else
        exec ./setup-plain.sh
    fi
elif [ -t 0 ] || [ -p /dev/stdin ]; then
    exec ./setup-plain.sh
else
    echo "ERROR: No ircbot.conf found and no interactive terminal available."
    echo ""
    echo "Run the setup wizard with:"
    echo "  docker compose run --rm radiobot"
    exit 1
fi
