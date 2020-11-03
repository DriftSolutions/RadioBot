# RadioBot

Since I no longer have a lot of time to put into RadioBot I thought I would go ahead and release the source code so others can check it out and customize it to their needs.
I'll be the first to admit it's kind of a mess in places, the bot was originally in C before moving to C++ and is basically what I learned C/C++ on so it's not perfect.

## Compiling on Windows

In the IRCBot folder, load IRCBot.sln. A Windows build is going to be harder just because there are so many dependencies if you want to build everything. I have (and the solution is set up for) a folder c:\deps with a lib amd include folder and I keep all my deps in it like on a Linux directory structure. The core things you will need:<br />

Drift Standard Libraries: https://github.com/DriftSolutions/DSL<br />
OpenSSL: I recommend https://slproweb.com/products/Win32OpenSSL.html

## Compiling on Linux/Unix

1. Install dependencies, cmake, git, protoc (profobuf compiler), and core GNU C/C++ compiler/tools (build-essential on Debian systems). You can find most deps by looking in your distro at https://wiki.shoutirc.com/index.php/Installation - you will need the corresponding -dev/-devel packages of course.<br />
2. cd to v5 folder, run cmake . (Note: out of source tree builds not supported at this time)<br />
3. make -j<however many CPU cores you have><br />
