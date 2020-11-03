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
#include "libmumble.h"

extern BOTAPI_DEF * api;
extern int pluginnum;
extern Titus_Mutex hMutex;

#define MUMBLE_MAX_SERVERS 4
#define MUMBLE_MAX_TOKENS 8

class MyMumbleClient: public MumbleClient {
public:
	int server_index;
	string last_comment;

	virtual void OnTextMessage(MumbleProto::TextMessage tm);
	virtual void OnServerSync(MumbleProto::ServerSync ss);
	virtual void OnReject(MumbleProto::Reject r);
	virtual void OnPing(MumbleProto::Ping p);
	virtual void OnPermissionDenied(MumbleProto::PermissionDenied);

	void JoinMyChan();
	void UpdateComment();
};

struct MUMBLE_SERVER_CONFIG {
	char host[128];
	int port;
	char username[128];
	char password[128];
	char channel[128];
	char comment[2048];
	char image[MAX_PATH];
	char cert_fn[MAX_PATH];
	char tokens[MUMBLE_MAX_TOKENS][48];
	MyMumbleClient * cli;
};

struct MUMBLE_CONFIG {
	bool shutdown_now;
	MUMBLE_SERVER_CONFIG servers[MUMBLE_MAX_SERVERS];
	bool RequirePrefix;
};

extern MUMBLE_CONFIG mumble_config;

