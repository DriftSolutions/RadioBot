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


#include "../adj_plugin.h"
#define MP3_BUFFER_SIZE 65536

#if defined(_WIN32)
typedef int ssize_t;
//#define ssize_t SSIZE_T
#endif
#if defined(WIN32)
	//#define LINK_MPG123_DLL
	#if defined(DEBUG)
	#pragma comment(lib, "libmpg123_d.lib")
	#else
	#pragma comment(lib, "libmpg123.lib")
	#endif
#endif
#include <mpg123.h>

#if defined(USE_TAGLIB)
	#include <taglib.h>
	#include <tag.h>
	#include <fileref.h>
	#include <mpegfile.h>
	#include <id3v2tag.h>
#if TAGLIB_MAJOR_VERSION > 1 || (TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION >= 8)
	#define TAGLIB_HAVE_DEBUG
	#include <tdebuglistener.h>
#endif
	#if defined(WIN32)
		#if defined(DEBUG)
			#pragma comment(lib, "taglib_d.lib")
		#else
			#pragma comment(lib, "taglib.lib")
		#endif
	#endif
#endif

#define LIBSSMT_DLL
#include "../../../../Common/LibSSMT/libssmt.h"

extern Titus_Mutex tagMutex;
extern AD_PLUGIN_API * adapi;
