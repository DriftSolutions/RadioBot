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

#include "../Common/ircbot-config.h"
#ifndef ENABLE_MYSQL
#define ENABLE_MYSQL
#endif
#include <drift/dsl.h>
#include <sqlite3.h>
#include "../Common/tinyxml/tinyxml.h"
#include "../Common/libremote/libremote.h"
#include "../v5/src/user_flags.h"
#include <Windowsx.h>
#include "../Common/DriftWindowingToolkit/dwt.h"
#if defined(WIN32)
#include "resource.h"
#endif

#define MAX_CONNECTIONS 32
#define VERSION "5.0.0"
#define UVERSION L"5.0.0"

enum CLIENT_TABS {
	TAB_DJ,
	TAB_FIND,
	TAB_ADMIN,
	TAB_OPTIONS,
	TAB_ABOUT,
	TAB_HELP,
	NUM_TABS
};
extern const char * tab_names[NUM_TABS];

struct CONFIG_HOSTS {
	char name[128];
	char host[128];
	uint16 port, mode;
	char username[64];
	char pass[64];
};

#define MAX_FIND_RESULTS 100
struct FIND_RESULT {
	char * fn; ///< At a minimum fn must be set so the bot has something to display. The bot will automatically show only the filename is a full path is included.
	uint32 id; ///< Optional member the source plugin can use however it wants.
};
struct FIND_RESULTS {
	FIND_RESULT songs[MAX_FIND_RESULTS];
	int num_songs;
	bool have_more;

	int plugin;
	void (*free)(FIND_RESULTS * res);
};

struct FIND_PROVIDER {
	bool (*init)(char * errmsg, size_t errSize);
	void (*quit)();

	FIND_RESULTS * (*find)(const char * query, uint32 search_id);
};

struct CONFIG {
	bool shutdown_now;
	HINSTANCE hInst;
	HWND mWnd;
	HWND tWnd[NUM_TABS];
	HWND sWnd;

	struct {
		char mysql_host[128];
		char mysql_user[128];
		char mysql_pass[128];
		char mysql_db[128];
		int mysql_port;
	} sam;
	struct {
		char folder[MAX_PATH];
		sqlite3 * dbHandle;
	} music_db;

	CONFIG_HOSTS hosts[MAX_CONNECTIONS];
	int host_index;
	bool connect;

	bool connect_on_startup;
	bool beep_on_req;
	bool keep_on_top;
};

void UpdateMusicDBStatus(HWND hWnd=NULL);
bool MusicDB_Init();
void MusicDB_Quit();
FIND_RESULTS * MusicDB_Find(const char * query, uint32 search_id);
void MusicDB_Scan();
void MusicDB_Clear();

bool sam_init(char * errmsg, size_t errSize);
void sam_quit();
void sam_sync();
TT_DEFINE_THREAD(Folder_Import);
TT_DEFINE_THREAD(SAM_Import);

extern CONFIG config;
extern RemoteClient * rc;
extern Titus_Mutex hostMutex;

void DoConnect(int newindex);
bool IsConnected();
void Disconnect();
void SendPacket(REMOTE_HEADER * rHead, const char * data=NULL);
void SendPacket(REMOTE_COMMANDS_C2S cmd, uint32 datalen=0, const char * data=NULL);
void SendPacket(REMOTE_COMMANDS_S2C cmd, uint32 datalen = 0, const char * data = NULL);

bool InputBox(HWND hParent, const char * query, char * buf, size_t bufSize);
HWND CreateTabDJ(HWND hParent);
HWND CreateTabFind(HWND hParent);
HWND CreateTabAdmin(HWND hParent);
HWND CreateTabOptions(HWND hParent);
HWND CreateTabAbout(HWND hParent);
HWND CreateTabHelp(HWND hParent);
void AddToolHelp(HWND hTip, HWND hParent, int id, TCHAR * text);
HWND CreateToolHelp(HWND hParent);
void SetWindowOnTop();

bool LoadConfig();
void SaveConfig();

bool OpenConnManager(HWND hParent);
bool OpenAboutBox(HWND hParent);

bool uflag_have_any_of(uint32 flags, uint32 wanted);
void uflag_to_string(uint32 flags, char * buf, size_t bufSize);

struct BOT_REPLY {
	REMOTE_HEADER rHead;
	char * data;
};

/* Old Stuff */

typedef struct {
	char name[128];
	char host[128];
	uint16 port, mode;
	char username[64];
	char pass[64];
} CONFIG_HOSTS_OLD4;

typedef struct {
	char revision;
	int index;
	T_SOCKET * sock;
	CONFIG_HOSTS_OLD4 hosts[MAX_CONNECTIONS];
	bool beep_on_req;
	bool keep_on_top;
} CONFIG_OLD4;

typedef struct {
	char host[256];
	char username[256];
	char pass[256];
	int port;
	SOCKET sock;
} CONFIG_OLD3;

typedef struct {
	char host[128];
	int port;
} CONFIG_HOSTS_OLD2;

typedef struct {
	char revision;
	char username[64];
	char pass[64];
	int index;
	T_SOCKET * sock;
	CONFIG_HOSTS_OLD2 hosts[16];
	bool ssl;
} CONFIG_OLD2;

typedef struct {
	char revision;
	int index;
	T_SOCKET * sock;
	CONFIG_HOSTS hosts[MAX_CONNECTIONS];
} CONFIG_OLD;

