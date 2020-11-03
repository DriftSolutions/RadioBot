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

#include "../../src/plugins.h"

extern BOTAPI_DEF *api;
extern int pluginnum;

#define CLIENT	"src"
#define VERSION "1.1"

struct LFM_CONFIG {
	bool enabled, handshaked, source_only;
	char user[128];
	char pass[128];
	char sesid[128];

	char np_url[256];
	char sub_url[256];
};
extern LFM_CONFIG lfm_config;

bool lfm_handshake();
bool lfm_scrobble(const char * artist, const char * album, const char * title, int songLen, time_t startTime);
