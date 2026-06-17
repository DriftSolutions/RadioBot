#!/bin/bash
# Plain read-based setup wizard (no TTY required, just stdin)
set -e

CONFIG="/radiobot/data/ircbot.conf"
TEXTFILE="/radiobot/data/ircbot.text"

echo ""
echo "========================================"
echo "   RadioBot First-Time Setup Wizard"
echo "========================================"
echo "  (run with a TTY for the graphical UI)"
echo ""

prompt() {
    local var="$1" msg="$2" default="$3"
    if [ -n "$default" ]; then
        read -rp "$msg [$default]: " val
        printf -v "$var" '%s' "${val:-$default}"
    else
        while true; do
            read -rp "$msg: " val
            if [ -n "$val" ]; then
                printf -v "$var" '%s' "$val"
                break
            fi
            echo "  (required)"
        done
    fi
}

echo "=== Bot settings ==="
prompt BOT_NICK "Bot IRC nickname" "RadioBot"
echo ""

echo "=== IRC Server ==="
prompt IRC_HOST "IRC server hostname" ""
prompt IRC_PORT "Port" "6667"
prompt IRC_CHAN  "Channel to join (e.g. #music)" ""
read -rp "Server password [blank = none]: " IRC_PASS
read -rp "TLS (0=no, 1=yes, 2=yes skip cert verify) [0]: " IRC_TLS
IRC_TLS="${IRC_TLS:-0}"
echo ""

echo "=== Streaming Server ==="
echo "  Types: shoutcast  shoutcast2  icecast  steamcast"
prompt SS_TYPE  "Server type" "shoutcast"
prompt SS_HOST  "Server hostname" ""
prompt SS_PORT  "Server port" "8000"
read -rp "Admin username [admin]: " SS_USER
SS_USER="${SS_USER:-admin}"
read -rp "Admin password [blank = none]: " SS_PASS

SS_MOUNT=""
if [[ "$SS_TYPE" == "icecast" || "$SS_TYPE" == "steamcast" ]]; then
    read -rp "Mount point [/]: " SS_MOUNT
    SS_MOUNT="${SS_MOUNT:-/}"
fi
echo ""

echo "=== Plugins ==="
echo "Common plugins (enter y/n for each):"
PLUGINS=()
plugin_ask() {
    local name="$1" desc="$2" default="$3"
    read -rp "  $name ($desc) [$default]: " ans
    ans="${ans:-$default}"
    [[ "$ans" == "y" ]] && PLUGINS+=("$name")
}
plugin_ask "Users_Shared"   "SQLite user/admin database" "y"
plugin_ask "Auto_Identify"  "NickServ auto-identify"     "y"
plugin_ask "ChanAdmin"      "Channel moderation"         "y"
plugin_ask "Welcome"        "Welcome message on join"    "n"
plugin_ask "Logging"        "Log IRC traffic to file"    "n"
plugin_ask "AutoDJ"         "AutoDJ music streaming"     "n"
plugin_ask "Telnet"         "Telnet remote control"      "n"
echo ""

{
cat <<EOF
// RadioBot configuration — generated $(date -u +"%Y-%m-%d %H:%M UTC")

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

module=0
for name in "${PLUGINS[@]}"; do
    case "$name" in
        AutoDJ)   echo "    Module${module} plugins/AutoDJ.so"   ;;
        SimpleDJ) echo "    Module${module} plugins/SimpleDJ.so" ;;
        *)        echo "    Module${module} plugins/${name}.so"  ;;
    esac
    (( module++ ))
done

echo "};"
} > "$CONFIG"

echo "Config written to $CONFIG"
echo "Starting RadioBot..."
exec ./radiobot --conf="$CONFIG" --text="$TEXTFILE"
