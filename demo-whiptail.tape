Output demo-whiptail.gif

Set Shell "bash"
Set FontSize 13
Set Width 1000
Set Height 650
Set Theme "Builtin Tango Dark"
Set Padding 20
Set PlaybackSpeed 1.5
Set TypingSpeed 50ms
Env COLORTERM truecolor
Env TERM xterm-256color

# Launch the whiptail wizard
Type "docker compose run --rm radiobot"
Sleep 1s
Enter
Sleep 5s

# Welcome screen — OK
Sleep 1s
Enter

# Bot nick — accept default RadioBot
Sleep 2s
Enter

# IRC intro screen — OK
Sleep 2s
Enter

# IRC host
Type "irc.radiobot-example.com"
Sleep 2s
Enter

# Port — accept default 6667
Sleep 2s
Enter

# Channel
Type "#radio"
Sleep 2s
Enter

# Server password — blank
Sleep 2s
Enter

# TLS — No
Tab
Sleep 2s
Enter

# Streaming intro screen — OK
Sleep 2s
Enter

# Streaming server type — navigate down to icecast (3rd option)
Down
Sleep 300ms
Down
Sleep 2s
Enter

# Streaming host
Type "icecast.radiobot-example.com"
Sleep 2s
Enter

# Port — accept default 8000
Sleep 2s
Enter

# Admin user — accept default
Sleep 2s
Enter

# Admin password
Type "changeme"
Sleep 2s
Enter

# Mount point (required for icecast)
Type "radio.mp3"
Sleep 2s
Enter

# Plugin checklist — also enable Welcome and AutoDJ
Down
Sleep 300ms
Down
Sleep 300ms
Down
Sleep 300ms
Space
Sleep 500ms
Down
Sleep 300ms
Down
Sleep 300ms
Space
Sleep 1500ms
Enter

# NickServ password (Auto_Identify is checked) — set one
Type "demopass123"
Sleep 1500ms
Enter

# AutoDJ music directory — accept the suggested default
Sleep 1500ms
Enter

# Final confirm — Yes
Sleep 2s
Enter

# "Config written, starting now!" — OK, then RadioBot boots and connects
Sleep 2s
Enter
Sleep 4s
