//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2026 Drift Solutions / Indy Sams            |
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

#ifndef MEMLEAK
#define MEMLEAK
#endif
#include "../../src/plugins.h"

#define LIBMOSQUITTO_STATIC
//extern "C" {
#include <mosquitto.h>
//}
#include <map>
#include <string>

extern BOTAPI_DEF * api;
extern int pluginnum;
extern DSL_Mutex hMutex;

struct MESHCORE_CONFIG {
	bool shutdown_now;

	bool RequirePrefix;
	bool EnableChannelMessages;
	bool EnableDirectMessages;
	bool SourceOnly;
	int SongInterval, SongIntervalSource;
	char Location[128];

	char host[128];
	int port;
	char username[128];
	char password[128];
	char topic_prefix[64];
	char self_node[128];
	char self_pubkey[65];
	bool connected;
	struct mosquitto * mosq;

	int64 lastReceivedContacts;
	int64 lastReceivedChannels;
	int64 lastPost;
};

struct MESHCORE_REF_HANDLE {
	bool is_channel;
	char channel_name[64];  // hashtag name, e.g. "#Primary" (from channel_info)
	char pubkey[128];
};

extern MESHCORE_CONFIG meshcore_config;

bool GetUserPubKey(const string& nick, string& pubkey, bool* was_from_db);
bool GetNickFromPubKeyPrefix(const string& pubkey, string& nick);
bool SaveUserPubKey(const string& nick, string& pubkey);
void AddPubKey(const string& nick, const string& pubkey, bool is_manual);

int MeshCore_UserDB_Commands(const char* command, const char* parms, USER_PRESENCE* ut, uint32 type);
//void sanitize_nick(char* nick);
string GetPubKeyPrefix(const string& pubkey);
string get_sanitized_nick(const string& str);
sql_rows GetTable(const char* query, char** errmsg = NULL);
