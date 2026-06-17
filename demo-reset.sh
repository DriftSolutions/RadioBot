#!/bin/bash
# Clears data volume so the wizard triggers on next run
docker run --rm -v "$(pwd)/data:/data" alpine sh -c "rm -f /data/ircbot.conf /data/ircbot.text"
echo "Reset done — data/ircbot.conf and data/ircbot.text removed"
