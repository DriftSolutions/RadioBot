//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2023 Drift Solutions / Indy Sams            |
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
#include <list>
#include <queue>

class ClaudeMessage {
public:
	string role;
	string content;
};

class ChatHistory {
public:
	ChatHistory(USER_PRESENCE * p) {
		Pres = p;
		RefUser(Pres);
	}
	~ChatHistory() {
		UnrefUser(Pres);
	}

	USER_PRESENCE * Pres;
	list<ClaudeMessage> messages;
	queue<string> new_lines;
	time_t lastSeen;
};

struct CLAUDE_CONFIG {
	bool shutdown_now;
	char api_key[256];
	char model[64];
	int32 max_tokens;

	int32 songMin, songMax;
	int32 songLeft;
};

extern CLAUDE_CONFIG claude_config;
extern BOTAPI_DEF *api;
extern int pluginnum; // the number we were assigned

extern DSL_Mutex chatMutex;
typedef map<string, shared_ptr<ChatHistory>, ci_less> chatListType;
extern chatListType sessions;
extern list<string> new_messages;

bool claude_query(const list<ClaudeMessage>& context, list<ClaudeMessage>& reply);

TT_DEFINE_THREAD(ChatThread);
