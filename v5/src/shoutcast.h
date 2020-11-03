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

#ifndef __INCLUDE_IRCBOT_SHOUTCAST_H__
#define __INCLUDE_IRCBOT_SHOUTCAST_H__

#if (LIBCURL_VERSION_MAJOR < 7) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 17)
#define OLD_CURL 1
//#error Ancient curl detected
#else
#undef OLD_CURL
#endif
size_t buffer_write(void *ptr, size_t size, size_t count, void *stream);

struct BUFFER {
	CURL * handle;
	CURLcode result;
	char * data;
	uint32 len;
#if defined(OLD_CURL)
	char *url, *userpwd;
#endif
};

#if defined(IRCBOT_ENABLE_SS)
#ifndef TIXML_USE_STL
#define TIXML_USE_STL
#endif
#include "../../Common/tinyxml/tinyxml.h"

int IfExistInt(TiXmlElement * m, char * name, int nDef = 0);
bool IfExistCopy(TiXmlElement * m, char * name, char * buf, int bufSize);

bool Prepare_SC(int num, BUFFER * buf);
bool Prepare_SC2(int num, BUFFER * buf);
bool Prepare_Steamcast(int num, BUFFER * buf);
bool Prepare_ICE(int num, BUFFER * buf);
bool Parse_SC(int num, BUFFER * buf, STATS * stats);
bool Parse_SC2(int num, BUFFER * buf, STATS * stats);
bool Parse_Steamcast(int num, BUFFER * buf, STATS * stats);
bool Parse_ICE(int num, BUFFER * buf, STATS * stats);

THREADTYPE scThread(void *lpData);
#endif // defined(IRCBOT_ENABLE_SS)

#endif
