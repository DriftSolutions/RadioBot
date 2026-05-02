//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2020 Drift Solutions / Indy Sams            |
|        More information available at https://www.shoutirc.com        |
|                                                                      |
|                    This file is part of RadioBot.                    |
|                                                                      |
|   RadioBot is free software: you can redistribute it and/or modify   |
| it under the terms of the GNU General Public License as published by |
|  the Free Software Foundation, either version 3 of the License, or   |
|                 (at your option) any later version.                  |
|                                                                      |
|     RadioBot is distributed in the hope that it will be useful,      |
|    but WITHOUT ANY WARRANTY; without even the implied warranty of    |
|     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the     |
|             GNU General Public License for more details.             |
|                                                                      |
|  You should have received a copy of the GNU General Public License   |
|  along with RadioBot. If not, see <https://www.gnu.org/licenses/>.   |
\**********************************************************************/
//@AUTOHEADER@END@

#define DSL_STATIC
#ifndef MEMLEAK
#define MEMLEAK
#endif
#include <drift/dsl.h>
#include <list>
#include <math.h>
#if defined(WIN32)
#include <sys/utime.h>
#else
#include <utime.h>
#include <getopt.h>
#endif

#if !defined(__ssize_t_defined) && !defined(_SSIZE_T_DECLARED) && !defined(_SSIZE_T)
//typedef int ssize_t;
#endif

#if defined(WIN32)
	//#define LINK_MPG123_DLL
	#if defined(DEBUG)
	#pragma comment(lib, "libmpg123_d.lib")
	#else
	#pragma comment(lib, "libmpg123.lib")
	#endif
#endif
#if defined(_WIN32)
typedef int ssize_t;
#endif
#include <mpg123.h>
#include <tag.h>
#include <fileref.h>

#include <lame/lame.h>
#include <mpegfile.h>
#include <id3v1tag.h>
#include <id3v2tag.h>

#if defined(WIN32)
	#if defined(DEBUG)
		#pragma comment(lib, "taglib_d.lib")
	#else
		#pragma comment(lib, "taglib.lib")
	#endif
#endif

struct CONFIG {
	FILE * log_fp;
	char src_dir[MAX_PATH];
	char dest_dir[MAX_PATH];
	int verbosity;

	int bitrate, samplerate, channels;
};
extern CONFIG config;

int ds_printf(int level, char * fmt, ...);
bool copy_id3(const char * src, const char * dest);
bool copy_mp3(const char * src, const char * dest, int64 size);
