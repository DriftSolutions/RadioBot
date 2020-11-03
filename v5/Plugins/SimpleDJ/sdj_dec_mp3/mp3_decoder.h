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
#define MP3_MAX_FRAME_SIZE 3456

#if defined(USE_TAGLIB)
#include <tag.h>
#include <fileref.h>
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

//extern MAD_INTERFACE mad;
extern Titus_Mutex tagMutex;
extern SD_PLUGIN_API * api;
//short fixed_to_16(mad_fixed_t sample);

//old decoder
bool mp3_open(READER_HANDLE * fnIn, TITLE_DATA * meta);
int mp3_decode();
void mp3_close();
