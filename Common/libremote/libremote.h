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

#if !defined(__INCLUDE_IRCBOT_PLUGINS_H__)
#ifndef DSL_STATIC
#define DSL_STATIC
#endif
#ifndef ENABLE_OPENSSL
#define ENABLE_OPENSSL
#endif
#ifndef MEMLEAK
#define MEMLEAK
#endif
#include <drift/dsl.h>
#include "../remote_protocol.h"

struct REMOTE_HEADER {
	union {
		REMOTE_COMMANDS_C2S ccmd; ///< The command to send to the bot
		REMOTE_COMMANDS_S2C scmd; ///< Reply from bot to client
	};
	uint32 datalen;
};

#define MAX_HOSTMASKS 16
struct IBM_USEREXTENDED {
	char nick[128];
	char pass[128];
	int32 ulevel_old_for_v4_binary_compat;
	uint32 flags;
	int32 numhostmasks;
	char hostmasks[MAX_HOSTMASKS][128];
};
#endif

#define LIBREMOTE_VERSION		"1.03"
#define PROTO_VERSION			0x17
#ifndef MAX_REMOTE_PACKET_SIZE
#define MAX_REMOTE_PACKET_SIZE	4096
#endif

typedef int (*rprintftype)(void * uptr, const char * fmt, ...);

class RemoteClient {
private:
	Titus_Sockets * socks;
	Titus_Mutex * hMutex;
	rprintftype rprintf;

	char client_name[64];
	char * user;
	char * pass;
	char * host;
	int32 port, ulevel;
	uint32 uflags;
	bool ircbotv5;
	void * uptr;
	bool ssl, ready, ssl_enabled;
	TITUS_SOCKET * sock;

public:
	RemoteClient();
	~RemoteClient();

	bool Connect(const char * host, int32 port, const char * user, const char * pass, int mode);// 0 = Try SSL, but it's OK if not able, 1 = Try SSL, fail if it doesn't work, 2 = Plain connection, don't try SSL
	bool IsReady();
	void Disconnect();

	bool SendPacket(REMOTE_HEADER * rHead, const char * buf);
	bool RecvPacket(REMOTE_HEADER * rHead, char * buf, uint32 bufSize);
	bool SendPacketAwaitResponse(REMOTE_HEADER * rHead, char * buf, uint32 bufSize);

	int GetUserLevel();
	uint32 GetUserFlags();
	bool ConnectedWithSSL();
	bool IsIRCBotv5();
	int  WaitForData(timeval * timeo);//return is just like select()
	int  WaitForData(int32 timeout_ms);//return is just like select()

	void SetClientName(const char * new_name);
	void SetOutput(rprintftype pOutput);
	void SetUserPtr(void * uuptr);
};


