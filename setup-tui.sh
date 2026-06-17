#!/bin/bash
# whiptail-based first-time setup wizard
set -e

CONFIG="/radiobot/data/ircbot.conf"
TEXTFILE="/radiobot/data/ircbot.text"
TITLE="RadioBot Setup"
W=70  # dialog width

# Redirect helper: whiptail writes its UI to stderr, value to stdout.
# We swap them so the box shows on screen and we capture the value.
wt() { whiptail "$@" 3>&1 1>&2 2>&3; }

err() { whiptail --title "Error" --msgbox "$1" 8 $W; }

require() {
    local val="$1" label="$2"
    if [ -z "$val" ]; then
        err "$label cannot be empty."
        return 1
    fi
}

has_plugin() {
    local name="$1" p
    for p in $PLUGIN_LIST; do
        [ "$p" = "$name" ] && return 0
    done
    return 1
}

# ---- Welcome ----
whiptail --title "$TITLE" --msgbox \
"Welcome to RadioBot!

This wizard will create your initial ircbot.conf.
All settings can be changed later by editing:

  ./data/ircbot.conf

Press OK to continue." 14 $W

# ========== BASE ==========
BOT_NICK=$(wt --title "$TITLE — Bot" \
    --inputbox "IRC nickname for the bot:" 8 $W "RadioBot") || exit 0
require "$BOT_NICK" "Nickname" || exec "$0"

# ========== IRC SERVER ==========
whiptail --title "$TITLE" --msgbox \
"Next: IRC Server

Enter the IRC network your bot should connect to." 9 $W

while true; do
    IRC_HOST=$(wt --title "$TITLE — IRC Server" \
        --inputbox "IRC server hostname:" 8 $W "") || exit 0
    require "$IRC_HOST" "IRC hostname" && break
done

IRC_PORT=$(wt --title "$TITLE — IRC Server" \
    --inputbox "Port:" 8 40 "6667") || exit 0
IRC_PORT="${IRC_PORT:-6667}"

while true; do
    IRC_CHAN=$(wt --title "$TITLE — IRC Server" \
        --inputbox "Channel to join (e.g. #music):" 8 $W "") || exit 0
    require "$IRC_CHAN" "Channel" && break
done

IRC_PASS=$(wt --title "$TITLE — IRC Server" \
    --passwordbox "Server password (blank = none):" 8 $W) || exit 0

IRC_TLS=0
if wt --title "$TITLE — IRC Server" \
    --yesno "Use TLS/SSL for the IRC connection?" 8 $W; then
    IRC_TLS=$(wt --title "$TITLE — IRC TLS" \
        --menu "TLS mode:" 12 $W 2 \
        "1" "Verify server certificate" \
        "2" "Skip certificate verification") || exit 0
fi

# ========== STREAMING SERVER ==========
whiptail --title "$TITLE" --msgbox \
"Next: Streaming Server

RadioBot reads now-playing info from your
SHOUTcast or Icecast server to post in IRC." 11 $W

SS_TYPE=$(wt --title "$TITLE — Streaming Server" \
    --menu "Server type:" 13 $W 4 \
    "shoutcast"  "SHOUTcast v1" \
    "shoutcast2" "SHOUTcast v2" \
    "icecast"    "Icecast / Icecast2" \
    "steamcast"  "SteamCast") || exit 0

while true; do
    SS_HOST=$(wt --title "$TITLE — Streaming Server" \
        --inputbox "Server hostname:" 8 $W "") || exit 0
    require "$SS_HOST" "Streaming server hostname" && break
done

SS_PORT=$(wt --title "$TITLE — Streaming Server" \
    --inputbox "Port:" 8 40 "8000") || exit 0
SS_PORT="${SS_PORT:-8000}"

SS_USER=$(wt --title "$TITLE — Streaming Server" \
    --inputbox "Admin username:" 8 $W "admin") || exit 0
SS_USER="${SS_USER:-admin}"

SS_PASS=$(wt --title "$TITLE — Streaming Server" \
    --passwordbox "Admin password (blank = none):" 8 $W) || exit 0

SS_MOUNT=""
if [[ "$SS_TYPE" == "icecast" || "$SS_TYPE" == "steamcast" ]]; then
    SS_MOUNT=$(wt --title "$TITLE — Streaming Server" \
        --inputbox "Mount point:" 8 $W "/") || exit 0
    SS_MOUNT="${SS_MOUNT:-/}"
fi

# ========== PLUGINS ==========
PLUGIN_CHOICES=$(wt --title "$TITLE — Plugins" \
    --checklist \
"Select plugins to enable.
(Space to toggle, Enter to confirm)
" 24 $W 16 \
    "Users_Shared"   "Remote shared user DB (needs another bot's admin port)" OFF \
    "Auto_Identify"  "Auto NickServ identify on connect"        ON  \
    "ChanAdmin"      "Channel moderation commands"              ON  \
    "Welcome"        "Welcome message on join"                  OFF \
    "Logging"        "Log IRC traffic to file"                  OFF \
    "AutoDJ"         "AutoDJ music streaming"                   OFF \
    "SimpleDJ"       "SimpleDJ (simpler stream control)"        OFF \
    "ChatGPT"        "ChatGPT AI integration"                   OFF \
    "Claude"         "Claude AI integration"                    OFF \
    "ChannelLinking" "Link channels across networks"            OFF \
    "LastFM"         "Last.fm now-playing integration"          OFF \
    "Telnet"         "Telnet remote control"                    OFF \
    "Trivia"         "Trivia game"                              OFF \
    "Hangman"        "Hangman game"                             OFF \
    "Uno"            "Uno card game"                            OFF \
    "Quotes"         "Quotes database"                          OFF \
) || exit 0
PLUGIN_LIST="${PLUGIN_CHOICES//\"/}"

# ========== PLUGIN-SPECIFIC SETTINGS ==========
# Some plugins need their own config section to do anything useful; ask
# only for the ones that were selected, and skip enabling them later if
# left blank rather than loading a plugin that just fails every boot.
AUTOIDENT_PASS=""
AUTOIDENT_NICKSERV="NickServ"
if has_plugin "Auto_Identify"; then
    AUTOIDENT_PASS=$(wt --title "$TITLE — Auto NickServ Identify" \
        --passwordbox "NickServ password (blank = disable this plugin):" 8 $W) || exit 0
fi

ADJ_CONTENT=""
if has_plugin "AutoDJ"; then
    ADJ_CONTENT=$(wt --title "$TITLE — AutoDJ" \
        --inputbox "Music directory (blank = configure later):" 8 $W "/radiobot/data/music") || exit 0
fi

SDJ_CONTENT=""
if has_plugin "SimpleDJ"; then
    SDJ_CONTENT=$(wt --title "$TITLE — SimpleDJ" \
        --inputbox "Music directory (blank = configure later):" 8 $W "/radiobot/data/music") || exit 0
fi

CHATGPT_KEY=""
if has_plugin "ChatGPT"; then
    CHATGPT_KEY=$(wt --title "$TITLE — ChatGPT" \
        --passwordbox "OpenAI API key (blank = disable this plugin):" 8 $W) || exit 0
fi

CLAUDE_KEY=""
if has_plugin "Claude"; then
    CLAUDE_KEY=$(wt --title "$TITLE — Claude" \
        --passwordbox "Anthropic API key (blank = disable this plugin):" 8 $W) || exit 0
fi

LASTFM_USER=""
LASTFM_PASS=""
if has_plugin "LastFM"; then
    LASTFM_USER=$(wt --title "$TITLE — Last.fm" \
        --inputbox "Last.fm username (blank = disable this plugin):" 8 $W) || exit 0
    if [ -n "$LASTFM_USER" ]; then
        LASTFM_PASS=$(wt --title "$TITLE — Last.fm" \
            --passwordbox "Last.fm password:" 8 $W) || exit 0
    fi
fi

# Plugins that were checked but are missing their required value get
# dropped here instead of being written into the config as a dead module.
ACTIVE_PLUGINS=()
for name in $PLUGIN_LIST; do
    case "$name" in
        Auto_Identify) [ -n "$AUTOIDENT_PASS" ] || continue ;;
        ChatGPT)       [ -n "$CHATGPT_KEY" ]    || continue ;;
        Claude)        [ -n "$CLAUDE_KEY" ]     || continue ;;
        LastFM)        { [ -n "$LASTFM_USER" ] && [ -n "$LASTFM_PASS" ]; } || continue ;;
    esac
    ACTIVE_PLUGINS+=("$name")
done

# ========== CONFIRM ==========
SUMMARY="Ready to write config:

  Bot nick  : $BOT_NICK
  IRC       : ${IRC_HOST}:${IRC_PORT}  channel ${IRC_CHAN}
  TLS       : ${IRC_TLS}
  Streaming : ${SS_TYPE}  ${SS_HOST}:${SS_PORT}
  Plugins   : ${ACTIVE_PLUGINS[*]}"

if ! wt --title "$TITLE — Confirm" --yesno "$SUMMARY

Write ircbot.conf and start RadioBot?" 19 $W; then
    whiptail --title "$TITLE" --msgbox "Setup cancelled. Re-run to try again." 8 $W
    exit 0
fi

# ========== WRITE CONFIG ==========
{
cat <<EOF
// RadioBot configuration — generated $(date -u +"%Y-%m-%d %H:%M UTC")
// Full documentation: https://wiki.shoutirc.com

Base {
    Nick $BOT_NICK
    LogFile /radiobot/data/radiobot.log
    PIDFile /radiobot/data/radiobot.pid
    Fork 0
};

SS {
    Server0 {
        Host $SS_HOST
        Port $SS_PORT
        User $SS_USER
EOF
[ -n "$SS_PASS" ]  && echo "        Pass $SS_PASS"
[ -n "$SS_MOUNT" ] && echo "        Mount $SS_MOUNT"
cat <<EOF
        Type $SS_TYPE
    };
};

IRC {
    Server0 {
        Host $IRC_HOST
        Port $IRC_PORT
EOF
[ -n "$IRC_PASS" ] && echo "        Pass $IRC_PASS"
cat <<EOF
        TLS $IRC_TLS

        Channel0 {
            Channel $IRC_CHAN
        };
    };
};

Plugin {
EOF

# Emit one Module=N line per active plugin
module=0
for name in "${ACTIVE_PLUGINS[@]}"; do
    case "$name" in
        AutoDJ)    echo "    Module${module} plugins/AutoDJ.so"   ;;
        SimpleDJ)  echo "    Module${module} plugins/SimpleDJ.so" ;;
        *)         echo "    Module${module} plugins/${name}.so"  ;;
    esac
    module=$(( module + 1 ))
done

echo "};"

# ---- Per-plugin sections (only emitted for plugins that ended up active) ----
if [ -n "$AUTOIDENT_PASS" ]; then
cat <<EOF

AutoIdentify {
    NickServ $AUTOIDENT_NICKSERV
    Pass $AUTOIDENT_PASS
};
EOF
fi

if has_plugin "AutoDJ"; then
cat <<EOF

AutoDJ {
    Server {
EOF
[ -n "$ADJ_CONTENT" ] && echo "        Content $ADJ_CONTENT"
[ -n "$SS_PASS" ]     && echo "        password $SS_PASS"
cat <<EOF
    };
    Options {
    };
};
EOF
fi

if has_plugin "SimpleDJ"; then
cat <<EOF

SimpleDJ {
    Server {
EOF
[ -n "$SDJ_CONTENT" ] && echo "        Content $SDJ_CONTENT"
[ -n "$SS_PASS" ]     && echo "        password $SS_PASS"
cat <<EOF
    };
    Options {
        QueuePlugin plugins/SimpleDJ/sdjq_memory.so
    };
};
EOF
fi

if [ -n "$CHATGPT_KEY" ]; then
cat <<EOF

ChatGPT {
    API_Key $CHATGPT_KEY
};
EOF
fi

if [ -n "$CLAUDE_KEY" ]; then
cat <<EOF

Claude {
    API_Key $CLAUDE_KEY
};
EOF
fi

if [ -n "$LASTFM_USER" ] && [ -n "$LASTFM_PASS" ]; then
cat <<EOF

LastFM {
    User $LASTFM_USER
    Pass $LASTFM_PASS
};
EOF
fi
} > "$CONFIG"

whiptail --title "$TITLE" --msgbox \
"Config written to:
  $CONFIG

RadioBot is starting now!" 10 $W

exec ./radiobot --conf="$CONFIG" --text="$TEXTFILE"
