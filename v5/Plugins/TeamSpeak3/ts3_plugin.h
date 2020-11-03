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

//#define DEBUG_MUTEX
#ifndef MEMLEAK
#define MEMLEAK
#endif
#include "../../src/plugins.h"
#include "libts3.h"

extern BOTAPI_DEF * api;
extern int pluginnum;
extern Titus_Mutex hMutex;

#define TS3_MAX_SERVERS 4
#define TS3_MAX_TOKENS 8

class MyTS3_Client: public TS3_Client {
public:
	int server_index;
	string last_comment;

	void OnPM(string snick, int nick_id, string smsg);
	void OnChan(string snick, int nick_id, int channel_id, string channel_name, string smsg);
	void UpdateTopic(STATS * stats);
};

struct TS3_SERVER_CONFIG {
	char host[128];
	int port;
	int server_id;
	char username[128];
	char password[128];
	char channel[128];
	char nick[128];
	//char channel_desc[2048];
	//char image[MAX_PATH];
	//char tokens[TS3_MAX_TOKENS][128];
	MyTS3_Client * cli;
};

struct TS3_CONFIG {
	bool shutdown_now;
	TS3_SERVER_CONFIG servers[TS3_MAX_SERVERS];
	bool RequirePrefix;
};

extern TS3_CONFIG ts3_config;

