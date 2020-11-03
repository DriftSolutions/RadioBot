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
#include <loudmouth/loudmouth.h>

extern BOTAPI_DEF * api;
extern int pluginnum;
extern Titus_Mutex hMutex;
extern Titus_Sockets socks;

struct JABBER_CONFIG {
	bool shutdown_now;
	bool connected, reconnect;
	time_t lastRecv;

        LmConnection * connection;

	int MinLevel;
	bool RequirePrefix;

        char server[128];
        char user[128];
        char pass[128];
        char resource[128];
		int port;
};

extern JABBER_CONFIG j_config;

