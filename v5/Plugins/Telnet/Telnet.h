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

enum TELNET_STATUS {
	DS_CONNECTED,
	DS_NEED_PASS,
	DS_AUTHENTICATED,
	DS_DROP
};

#define TELNET_BUFFER_SIZE 1024

class TelnetSession {
private:
public:
	TelnetSession(T_SOCKET * sSock);
	~TelnetSession();

	USER_PRESENCE * Pres;
	USER * User;

	char buf[TELNET_BUFFER_SIZE];
	int bufind;
	int ctrl;
	bool console;

	TELNET_STATUS status;
	string username;
	time_t timestamp;
	T_SOCKET * sock;
};

struct TELNET_CONFIG {
	bool shutdown_now;
	char SSL_Cert[MAX_PATH];
	int Port;
	unsigned int MaxSessions;
	T_SOCKET * lSock;
};

extern TELNET_CONFIG telconf;
extern Titus_Sockets3 * sockets;
extern BOTAPI_DEF *api;
extern int pluginnum; // the number we were assigned

extern Titus_Mutex chatMutex;
typedef std::vector<TelnetSession *> chatListType;
extern chatListType sessions;

TT_DEFINE_THREAD(ChatThread);
USER_PRESENCE * alloc_telnet_presence(USER * user, T_SOCKET * sock);
USER_PRESENCE * alloc_telnet_presence(TelnetSession * ses);

