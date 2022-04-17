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


#ifndef __INCLUDE_IRCBOT_MAIN_H__
#define __INCLUDE_IRCBOT_MAIN_H__

#if !defined(IRCBOT_STANDALONE)
	#define IRCBOT_NAME	"RadioBot"
	#define IRCBOT_CONF	"ircbot.conf"
	#define IRCBOT_TEXT	"ircbot.text"
	#define IRCBOT_DB	"ircbot.db"
	#define IRCBOT_ENABLE_IRC 1
	#if !defined(IRCBOT_LITE)
	#define IRCBOT_ENABLE_SS 1
	#endif
#else
	#define IRCBOT_ENABLE_SS 1
	#define IRCBOT_NAME	"AutoDJ"
	#define IRCBOT_CONF	"autodj.conf"
	#define IRCBOT_TEXT	"autodj.text"
	#define IRCBOT_DB	"autodj.db"
#endif


#define MAKE_VERSION(Major,Minor,Flags) ((Major << 16)|(Minor << 8)|Flags)
#define GET_MAJOR(Version)		((Version & 0x00FF0000) >> 16)
#define GET_MINOR(Version)		((Version & 0x0000FF00) >> 8)
#define GET_FLAGS(Version)		 (Version & 0x000000FF)

#if !defined(_WIN32) && defined(WIN32)
#define _WIN32
#endif

#ifdef _WIN32
static inline void DRIFT_DIGITAL_SIGNATURE() {
	__asm {
		jmp sigend;
		_emit 0x44;
		_emit 0x52;
		_emit 0x49;
		_emit 0x46;
		_emit 0x54;
		_emit 0x53;
		_emit 0x49;
		_emit 0x47;
		_emit 0x00;
		_emit 0x00;
		_emit 0x00;
		_emit 0x00;
		_emit 0x00;
		_emit 0x00;
		_emit 0x99;
sigend:
	}
}
#elif defined(__GNUC__) && !defined(__clang__) && !defined(__llvm__) && !defined(__arm__)
static inline void DRIFT_DIGITAL_SIGNATURE() {
	__asm __volatile__ ("jmp sigend;\n\t"
		".byte 0x44;\n\t"
		".byte 0x52;\n\t"
		".byte 0x49;\n\t"
		".byte 0x46;\n\t"
		".byte 0x54;\n\t"
		".byte 0x53;\n\t"
		".byte 0x49;\n\t"
		".byte 0x47;\n\t"
		".byte 0x00;\n\t"
		".byte 0x00;\n\t"
		".byte 0x00;\n\t"
		".byte 0x00;\n\t"
		".byte 0x00;\n\t"
		".byte 0x00;\n\t"
		".byte 0x99;\n\t"
		"sigend:\n\t"
	);
}
#else
static inline void DRIFT_DIGITAL_SIGNATURE() {}
#endif

void ib_printf2(int pluginnum, const char * fmt, ...);
#define ib_printf(x, ...) ib_printf2(-1, x, ##__VA_ARGS__)
bool OpenLog();
void CloseLog();
void PrintToLog(const char * buf);

#define ENABLE_CURL_SSL
#define ENABLE_CURL
//#include <drift/dsl.h>

#ifdef USE_NEW_CONFIG
#define Universal_Config Universal_Config2
#undef DS_CONFIG_SECTION
#define DS_CONFIG_SECTION ConfigSection
#undef DS_VALUE
#define DS_VALUE ConfigValue
#endif

#include "plugins.h"
#include "plugins_private.h"
#include "common_messages.h"
#ifdef WIN32
#include "resource.h"
#endif

uint32 GetBotVersion();
const char * GetBotVersionString();

#include "db.h"
#include "users.h"
#include "util.h"
#include "textproc.h"
#include "language.h"
#include "botapi.h"
#include "SendQ.h"
#include "remote.h"
#include "commands.h"
#include "command_aliases.h"
#include "ial.h"
#include "irc.h"
//#include "mt.h"
//#include "well1024.h"
#include "shoutcast.h"

#if defined(IRCBOT_ENABLE_SS)
#include "requests.h"
#include "ratings.h"
#endif

typedef struct {
	uint8 comp;
	uint32 decomp_len;// (use header.datalen - sizeof(CONSOLE_HEADER) for compressed size)
} CONSOLE_HEADER;

#if defined(IRCBOT_ENABLE_SS)
class SOUND_SERVERS {
	public:
		std::string host, mount, user, pass;
		int port, streamid;
		/*
		bool has_source;
		int32 lastlisten;
		int32 lastmax;
		int32 lastpeak;
		*/
		SS_TYPES type;
};
#endif

typedef std::map<std::string, std::string, ci_less> message_list;

class BASE_CFG {
	public:
		void Reset();

#if defined(IRCBOT_ENABLE_IRC)
		std::string irc_nick;
		std::string log_chan;
		std::string log_chan_key;
#endif
		std::string log_file;
		std::string pid_file;
		std::string base_path;
		std::string myip;
		std::string bind_ip;
		std::string command_prefixes;
		std::string langCode;
		uint32 default_flags;
		FILE * log_fps[MAX_PLUGINS+1];

		bool AutoRegOnHello;
		bool Fork, Forked, OnlineBackup;
		int sendq;
		int backup_days;

#if defined(IRCBOT_ENABLE_SS)
		DJNAME_MODES dj_name_mode;
		int updinterval;
		bool enable_requests, enable_find;
		int expire_find_results;
		int multiserver;
		int max_find_results;
		REQUEST_MODES req_mode;
#if defined(IRCBOT_ENABLE_IRC)
		string req_fallback;
		std::string req_modes_on_login;
#endif

		//std::string playlist;
		bool anyserver, allow_pm_requests;

		bool EnableRatings;
		unsigned int MaxRating;
#endif

#if defined(IRCBOT_ENABLE_IRC)
		std::string quitmsg;
#endif

		std::string reg_name;
		std::string reg_key;

		std::string ssl_cert;
		std::string fnConf;
		std::string fnText;

		int argc;
		std::map<int, std::string> argv;
};

#if defined(IRCBOT_ENABLE_IRC)
class TIMERS {
	public:
		std::string action;
		std::vector<std::string> lines;
		int network;
		int32 delaymin, delaymax;
		int32 nextFire;
};
#endif

typedef struct {
	T_SOCKET * sock;
	uint16 port;
	bool uploads_enabled;
	char upload_path[MAX_PATH];
} REMOTE;

struct REMOTE_SESSION {
	REMOTE_SESSION *Prev,*Next;
	T_SOCKET * sock;
	USER * user;
	unsigned char cli_version;
};

class GLOBAL_CONFIG {
public:
	void Reset();

	Universal_Config * config;
	sqlite3 * hSQL;
#ifdef WIN32
	HINSTANCE hInst;
	T_SOCKET * cSock;
	int cPort;
#endif
	Titus_Sockets3 * sockets;

	bool shutdown_now;
	bool shutdown_reboot;
	bool shutdown_sendq;
	bool rehash;
	int32 unload_plugin;
	int64 start_time;
	int32 dospam;
#if defined(IRCBOT_ENABLE_IRC)
	int32 num_irc,num_timers,dotopic,doonjoin;
	string topicOverride;
	IRCNETS ircnets[MAX_IRC_SERVERS];
#endif

#if defined(IRCBOT_ENABLE_SS)
	int32 num_ss;
	SOUND_SERVERS s_servers[MAX_SOUND_SERVERS];
	STATS stats;
	STATS s_stats[MAX_SOUND_SERVERS];
	USER_PRESENCE * req_user;
	REQUEST_INTERFACE * req_iface;
#endif
	//MESSAGES messages[MAX_MESSAGES];
	message_list messages;
	message_list customVars;
#if defined(IRCBOT_ENABLE_IRC)
	TIMERS timers[MAX_TIMERS];
#endif

	BASE_CFG base;
	REMOTE remote;

	PLUGIN plugins[MAX_PLUGINS];

	REMOTE_SESSION *sesFirst,*sesLast;
	//USER *fUser, *lUser;
};

extern Titus_Mutex sesMutex;
extern GLOBAL_CONFIG config;

// functions in main.cpp
//GLOBAL_CONFIG * GetConfig();
int BotMain(int argc, char *argv[]);
bool LoadText(const char * fn);
bool SyncConfig(Universal_Config * con=NULL);
int voidptr2int(void * ptr);
void LoadReperm();
void backup_file(const char * ofn);
bool CheckConfig();

void config_online_backup();
bool make_tls_cert(const char * fn);

// functions in idle.cpp
THREADTYPE IdleThread(VOID *lpData);
void seed_randomizer();
uint32_t genrand_int32();
uint32_t genrand_range(uint32 iMin, uint32 iMax);

// functions in tcl.cpp
/*
bool TCL_Init();
bool TCL_IsLoaded();
void TCL_Quit();
int TCL_LoadScript(const char *fileName);
void TCL_LoadScripts();
//int TCL_Commands(char * command, char * parms, IBM_USER * ut, uint32 type, T_SOCKET * sock, CommandOutputType output);
*/

int TouchYP(YP_HANDLE * yp, const char * cur_playing);
void DelFromYP(YP_HANDLE * yp);
bool AddToYP(YP_HANDLE * yp, SOUND_SERVER * sc, YP_CREATE_INFO * ypinfo);

struct MEMWRITE {
	char * data;
	size_t len;
};
size_t memWrite(void *ptr, size_t size, size_t nmemb, void *stream); // CURL callback

#endif
