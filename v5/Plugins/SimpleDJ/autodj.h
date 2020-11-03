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


#define SIMPLEDJ_VERSION "1.0." BUILD_STRING ""

#include "adj_plugin.h"
#include "../TTS_Services/tts.h"

//#include "../../plugins.h"
//#include "mt.h"
#define PI 3.134

//#ifdef rand
//#undef rand
//#endif

#ifdef _WIN32
static inline void DRIFT_DIGITAL_SIGNATURE() {
	__asm {
		mov eax, 'D';
		mov eax, 'R';
		mov	eax, 'I';
		mov eax, 'F';
	}
}
#else
#define DRIFT_DIGITAL_SIGNATURE()
/*
static inline void DRIFT_DIGITAL_SIGNATURE() {
	__asm("mov 'D', %eax\nmov 'R', %eax\nmov 'I', %eax\nmov 'F', %eax\n");
}
*/
#endif

#include "read_file.h"
#include "read_stream.h"
#include "read_mms.h"

#include "ss_shoutcast.h"
#include "ss_ultravox.h"
#include "ss_icecast.h"
#include "ss_streamerp2p.h"

void FreeQueue(QUEUE * Scan);
QUEUE * CopyQueue(QUEUE * Scan);

//#include "Queue_Memory.h"
//#include "meta_cache.h"

struct SD_PLUGINS {
	SD_PLUGIN * plug;
	DL_HANDLE hHandle;
};

#define MAX_DECODERS 32
#define MAX_ENCODERS 32
#define MAX_TITLE_UPDATES 16
#define MAX_VOICE_MESSAGES 32
#define MAX_CONTENT_DIRS 16

struct SD_CONFIG_SERVER {
	char Pass[128];
	char Name[128];
	char Desc[128];
	char URL[256];
	char Mount[64];
	char Genre[64];
	char IRC[128];
	char ICQ[64];
	char AIM[64];

	char ContentDir[256*MAX_CONTENT_DIRS];
	char PromoDir[256];
	char MimeType[64];
	char EncoderName[64];

	bool Public, Reset;

	int32 Bitrate;
	int32 Channels;
	int32 Sample;
};

struct SD_CONFIG_OPTIONS {
	bool EnableFind, EnableRatings, EnableYP, EnableRequests, AutoStart;
	int32 AutoPlayIfNoSource;
	int32 MinReqTimePerSong;
	uint32 DoPromos;
	uint32 NumPromos;
	int32 AutoReload;
	uint32 NoRepeat;
	char MoveDir[256];
	char QueuePlugin[256];
	char NoPlayFilters[1024];
};

struct SD_CONFIG_VOICE {
	bool EnableVoice;
	char Artist[256];
	char Title[256];
	int32 NumMessages;
	char * Messages[MAX_VOICE_MESSAGES];
};

enum REPORT_TYPES {
	REP_TYPE_NONE,
	REP_TYPE_PRESENCE,
	REP_TYPE_REMOTE
};

struct REPORT {
	REPORT_TYPES type;
	char nick[64];
	USER_PRESENCE * pres;
	T_SOCKET * sock;
};

typedef std::vector<REPORT> countdownList;
extern countdownList countdowns;

struct SERVER {
	FEEDER * SelFeeder;
	SimpleDJ_Feeder * Feeder;

	char Pass[128];
	char Name[128];
	char Desc[128];
	char URL[256];
	char Mount[64];
	char Genre[64];
	char IRC[128];
	char ICQ[64];
	char AIM[64];

	char MimeType[64];
	//char EncoderName[64];

	bool Enabled, Public, Reset;

	int32 Bitrate;
	int32 Channels;
	int32 Sample;
};

struct SD_CONFIG {
	SD_CONFIG_SERVER Server;
	SD_CONFIG_FILELISTER FileLister;
	SD_CONFIG_OPTIONS Options;
	SD_CONFIG_VOICE Voice;

	int32 nofiles, nopromos;
	int32 cur_promo;
	bool force_promo;
	bool countdown;
	bool move;
	bool shutdown_now;
	bool connected;
	bool playing;
	bool next;
	int64 playagainat;
	char voice_override[1024];

	QUEUE * cursong;
	TIMER * Filter;

	int64 resumePos;
	QUEUE * resumeSong;

	USER_PRESENCE * repto;
	//int32 repnet;
	//char repto[32];
	//T_SOCKET * repsock;

	TTS_INTERFACE tts;

	DECODER * Decoders[MAX_DECODERS];
	//ENCODER * Encoders[MAX_ENCODERS];
	SERVER Servers[MAX_SOUND_SERVERS];
	//ENCODER * Encoder;
	MIXER * Mixer;
	//FEEDER  * Feeder;

	SD_QUEUE_PLUGIN * Queue;
	SD_PLUGINS Plugins[32];
	int32 noplugins;
};

// in simpledj.cpp
extern Titus_Sockets3 * sockets;
extern Titus_Mutex * QueueMutex;
extern SD_CONFIG sd_config;
DL_HANDLE getInstance();
extern BOTAPI_DEF * api;
extern int pluginnum;

READER_HANDLE * OpenFile(const char * fn, const char * mode);
bool ShouldIStop();
DECODER * GetFileDecoder(const char * fn);
bool AllowRequest(QUEUE * Scan);
int AddRequest(QUEUE * Scan, bool to_front=false);

bool MatchesNoPlayFilter(char * str);
bool MatchFilter(QUEUE * Scan, TIMER * Timer, char * pat);
char * PrettyTime(int32 seconds, char * buf);

//TITLE_DATA * AllocTitleData();
void QueueTitleUpdate(TITLE_DATA * td);
void urlencode(char * buf, unsigned long bufSize);

// in timing.cpp
void Timing_Reset(int32 SongLen);
void Timing_Add(int32 iTime);
void Timing_Done();
int32 GetTimeLeft(bool secs=false);

// in plugins.cpp
SD_PLUGIN_API * GetAPI();
void LoadPlugins(char * path, bool quiet=false);
void UnloadPlugins();
bool LoadQueuePlugin(char * fn);
void UnloadQueuePlugin();

// in schedule.cpp
THREADTYPE ScheduleThread(void * lpData);
void LoadSchedule();

// in mixer.cpp
MIXER * GetMixer();
/*
void MixerShutdown();
bool MixerStartup();
*/

// in req_iface.cpp
extern REQUEST_INTERFACE sdj_req_iface;
