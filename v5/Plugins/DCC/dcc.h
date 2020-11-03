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
#include <physfs.h>
#include <vector>

#if (PHYSFS_VER_MAJOR < 1 || (PHYSFS_VER_MAJOR == 1 && PHYSFS_VER_MINOR < 1))
	#define PHYSFS_mount(x,y,z) PHYSFS_addToSearchPath(x,z)
#endif

#define fseek #error
#define ftell #error

enum DCCT_STATUS {
	DS_DROP,
	DS_CONNECTING,
	DS_WAITING,
	DS_CONNECTED,
	DS_AUTHENTICATED,
	DS_TRANSFERRING
};

struct DCCT_QUEUE {
	DCCT_STATUS status;

	char nick[32];
	char fn[MAX_PATH];
	int64 size;
	int token; // for passive DCC

	int netno;
	time_t timestamp;

	int port;
	char ip[16];

	/*
		In receive mode, pos = how many bytes have been received
		In send mode, pos = the current position in the file
	*/
	int64 pos;

	DCCT_QUEUE * Next;
};

struct DCCT_CHAT {
	USER_PRESENCE * Pres;
	USER * User;//same as Pres->User
	//char nick[32];
	//char hostmask[128];
	//int level;
	//int netno;

	DCCT_STATUS status;
	time_t timestamp;

	T_SOCKET *sock, *lSock;
	char ip[NI_MAXHOST];
	int port;

	char cwd[1024];
};

struct DCCT_CONFIG {
	bool shutdown_now;

	unsigned int maxFServs, maxSends, maxRecvs;

	bool enable_chat;

	bool enable_recv;
	char recv_path[256];

	bool enable_fserv, AllowSymLinks;
	char fserv_trigger[32];
	char fserv_path[1024];

	bool enable_get;
	bool enable_autodj;
	char get_trigger[32];
	char get_path[256];

	int first_port, last_port;
	int last_random_port;
	char local_ip[NI_MAXHOST];
};

extern DCCT_CONFIG dcc_config;
extern Titus_Sockets3 * sockets;

extern Titus_Mutex sendMutex;
extern DCCT_QUEUE *SendQueue,*curSend;

THREADTYPE RecvThread(VOID *lpData);
THREADTYPE RecvPasvThread(VOID *lpData);
THREADTYPE SendThread(VOID *lpData);
THREADTYPE ChatThread_Incoming(VOID *lpData);
THREADTYPE ChatThread_Outgoing(VOID *lpData);
THREADTYPE FServThread(VOID *lpData);

const char * GetStatusText(DCCT_STATUS s);
int WaitXSecondsForSock(T_SOCKET * sock, int secs);
T_SOCKET * BindRandomPort();
void AddSend(DCCT_QUEUE * dNew);
int CommandOutput_DCC(T_SOCKET * sock, const char * dest, const char * str);

USER_PRESENCE * alloc_dcc_chat_presence(USER_PRESENCE * ut, T_SOCKET * sock);
USER_PRESENCE * alloc_dcc_chat_presence(DCCT_CHAT * trans);

extern BOTAPI_DEF *api;
extern int pluginnum; // the number we were assigned

typedef std::vector<DCCT_CHAT *> chatListType;
typedef std::vector<DCCT_QUEUE *> recvListType;
extern chatListType chatList;
extern recvListType recvList;
extern chatListType fservList;

#if PHYSFS_VER_MAJOR < 2 || (PHYSFS_VER_MAJOR == 2 && PHYSFS_VER_MINOR < 1)
#define PHYSFS_HAS_STAT 0
#else
#define PHYSFS_HAS_STAT 1
#endif

#if PHYSFS_HAS_STAT == 0
typedef enum PHYSFS_FileType
{
	PHYSFS_FILETYPE_REGULAR, /**< a normal file */
	PHYSFS_FILETYPE_DIRECTORY, /**< a directory */
	PHYSFS_FILETYPE_SYMLINK, /**< a symlink */
	PHYSFS_FILETYPE_OTHER /**< something completely different like a device */
} PHYSFS_FileType;

typedef struct PHYSFS_Stat
{
	PHYSFS_sint64 filesize; /**< size in bytes, -1 for non-files and unknown */
	PHYSFS_sint64 modtime;  /**< last modification time */
	PHYSFS_sint64 createtime; /**< like modtime, but for file creation time */
	PHYSFS_sint64 accesstime; /**< like modtime, but for file access time */
	PHYSFS_FileType filetype; /**< File? Directory? Symlink? */
	int readonly; /**< non-zero if read only, zero if writable. */
} PHYSFS_Stat;

int PHYSFS_stat(const char *fname, PHYSFS_Stat * st);
#endif

struct PHYSFS_MyStat {
	PHYSFS_Stat st;
	char real_fn[MAX_PATH];
};
int PHYSFS_myStat(const char *fname, PHYSFS_MyStat * st);
