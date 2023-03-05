# RadioBot

Since I no longer have a lot of time to put into RadioBot I thought I would go ahead and release the source code so others can check it out and customize it to their needs.
I'll be the first to admit it's kind of a mess in places, the bot was originally in C before moving to C++ and is basically what I learned C/C++ on so it's not perfect.

## Compiling on Windows

In the IRCBot folder, load IRCBot.sln. A Windows build is going to be harder just because there are so many dependencies if you want to build everything. I have (and the solution is set up for) a folder c:\deps with a lib and include folder and I keep all my deps in it like on a Linux directory structure. The core things you will need:<br />

Drift Standard Libraries: https://github.com/DriftSolutions/DSL<br />
OpenSSL: I recommend https://slproweb.com/products/Win32OpenSSL.html

## Compiling on Linux/Unix

1. Install dependencies, cmake, git, protoc (profobuf compiler), and core GNU C/C++ compiler/tools (build-essential on Debian systems). You can find most deps by looking in your distro at https://wiki.shoutirc.com/index.php/Installation - you will need the corresponding -dev/-devel packages of course.<br />
```On Ubuntu 22.10: sudo apt install libssl-dev libsqlite3-dev libwxgtk3.2-dev libtag1-dev libmp3lame-dev libogg-dev libvorbis-dev libsndfile1-dev libavcodec-extra libavformat-dev libavcodec-dev libcurl4-openssl-dev libmpg123-dev libresample1-dev libncurses5-dev libphysfs-dev libpcre3-dev libprotobuf-dev libmysqlclient-dev libfaac-dev libopus-dev libloudmouth1-dev libdbus-glib-1-dev libmuparser-dev libsoxr-dev build-essential cmake libz-dev git protobuf-compiler```<br />
```On Debian 11: sudo apt install libssl-dev libsqlite3-dev libwxgtk3.0-gtk3-dev libtag1-dev libmp3lame-dev libogg-dev libvorbis-dev libsndfile1-dev libavcodec-extra libavformat-dev libavcodec-dev libcurl4-openssl-dev libmpg123-dev libresample1-dev libncurses5-dev libphysfs-dev libpcre3-dev libprotobuf-dev libopus-dev libloudmouth1-dev libdbus-glib-1-dev libmuparser-dev libsoxr-dev build-essential cmake libz-dev git protobuf-compiler default-libmysqlclient-dev libfaac-dev autoconf libtool-bin liblua5.4-dev```<br />
  (Please let me know if I missed any.)
2. cd to the cloned repo, create a 'build' subfolder and cd to it.<br />
3. Run: cmake ..<br />
4. make -j&lt;however many CPU cores you have&gt;<br />
