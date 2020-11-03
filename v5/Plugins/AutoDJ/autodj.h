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

#ifndef __AUTODJ_H__
#define __AUTODJ_H__

#define AUTODJ_VERSION "3.0." BUILD_STRING ""
#define _WIN32_WINNT 0x0501

#include "adj_plugin.h"
#include "../TTS_Services/tts.h"

#if defined(WIN32)
#include <VersionHelpers.h>
#endif

//#include "../../plugins.h"
//#include "mt.h"
#define PI 3.134

//#ifdef rand
#undef rand
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

#include "ss_shoutcast.h"
#include "ss_ultravox.h"
#include "ss_icecast.h"
#include "ss_streamerp2p.h"
#include "ss_steamcast.h"

QUEUE * AllocQueue();
void FreeQueue(QUEUE * Scan);
QUEUE * CopyQueue(const QUEUE * Scan);

//#include "Queue_Memory.h"
//#include "meta_cache.h"

struct AD_PLUGINS {
	AD_PLUGIN * plug;
	DL_HANDLE hHandle;
};

#define MAX_DECODERS 32
#define MAX_ENCODERS 32
#define MAX_TITLE_UPDATES 16
#define MAX_VOICE_MESSAGES 32
#define MAX_CONTENT_DIRS 16

struct AD_CONFIG_SERVER {
	char Pass[128];
	char Name[128];
	char Desc[128];
	char URL[256];
	char Mount[64];
	char Genre[64];
	char IRC[128];
	char ICQ[64];
	char AIM[64];

	//char ContentDir[256*MAX_CONTENT_DIRS];
	char PromoDir[256*MAX_CONTENT_DIRS];
	char MimeType[64];
	char EncoderName[64];

	bool Public, Reset;

	int32 Bitrate;
	int32 Channels;
	int32 Sample;

	char SourceIP[128];
	int32 SourcePort;
};

struct AD_CONFIG_OPTIONS {
	//AUTODJ_MODES Mode;
	bool EnableFind, EnableYP, EnableRequests, AutoStart, EnableCrossfade, ScanDebug, CheckNewFilesOnly, OnlyScanNewFiles, EnableTitleUpdates, StrictParse, IncludeAlbum;
	char ResamplerPlugin[256];
	int32 AutoPlayIfNoSource;
	int32 ID3_Mode;
	int32 MinReqTimePerSong;
	int32 MinReqTimePerUser;
	int32 MinReqTimePerUserMask;
	int32 MinReqTimePerArtist;
	int32 MaxRequests;
	uint32 MaxSongDuration;
	int32 AutoReload;
	int32 DJConnectTime;
	int32 ScanThreads;
	uint32 RequestDelay;
	int32 DoPromos,DoPromosMax;
	uint32 NumPromos;
	uint32 NoRepeat, NoRepeatArtist;
	int32 CrossfadeMinDuration, CrossfadeLength;
	char YouTubeDir[256];
	char YouTubeExtra[256];
	char MoveDir[256];
	char QueuePlugin[256];
	char RotationPlugin[256];
	char DSPPlugin[256];
	char NoPlayFilters[1024];
	double Volume;
};

struct AD_CONFIG_VOICE {
	bool EnableVoiceBroadcast;
	int32 DoVoice,DoVoiceMax;
	bool DoVoiceOnRequests;
	char Artist[256];
	char Title[256];
	int32 NumMessages;
	char * Messages[MAX_VOICE_MESSAGES];
};

struct AD_CONFIG_PLAYLIST {
	char Name[32];
	char ContentDir[256*MAX_CONTENT_DIRS];
};

struct AD_CONFIG_PLORDER {
	int MinPlays, MaxPlays;
	int Playlist;
};

typedef std::vector<USER_PRESENCE *> countdownList;
extern countdownList countdowns;

struct AD_CONFIG {
	AD_CONFIG_SERVER Server;
	AD_CONFIG_FILELISTER FileLister;
	AD_CONFIG_OPTIONS Options;
	AD_CONFIG_VOICE Voice;

	AD_CONFIG_PLAYLIST Playlists[MAX_PLAYLISTS];
	AD_CONFIG_PLORDER PlaylistOrder[MAX_PLAYLISTS];
	int32 num_playlists, num_playlist_orders;

	int pluginnum;
	BOTAPI * api;

	//TITLE_DATA * TitleUpdates[MAX_TITLE_UPDATES];
	Titus_Sockets3 * sockets;

	int32 num_files, num_promos;
	int32 cur_promo;
	bool force_promo;
	bool countdown;
	bool countdown_now;
	bool shutdown_now;
	bool connected;
	bool playing;
	bool reloading;
	bool next;
	int64 playagainat;
	char voice_override[1024];

	QUEUE * cursong;
	QUEUE * nextSong;
	TIMER * Filter;

	//int64 resumePos;
	//QUEUE * resumeSong;
#if defined(WIN32)
	ITaskbarList3 *pTaskList;
#endif
	USER_PRESENCE * repto;
	USER * lock_user;

	TTS_INTERFACE tts;

	DECODER * Decoders[MAX_DECODERS];
	ENCODER * Encoders[MAX_ENCODERS];

	//MIXER * Mixer;

	ENCODER * SelEncoder;
	FEEDER * SelFeeder;
	AutoDJ_Encoder * Encoder;
	AutoDJ_Feeder * Feeder;

	Deck * Decks[NUM_DECKS];

	AD_QUEUE_PLUGIN * Queue;
	AD_ROTATION_PLUGIN * Rotation;
	AD_RESAMPLER_PLUGIN * Resampler;
	AD_PLUGINS Plugins[32];
	int32 noplugins;
};

// in autodj.cpp
extern BOTAPI * api;
extern int pluginnum;
DL_HANDLE getInstance();
bool AllowSongPlayback(const QUEUE * Scan, time_t tme, const char * pat);

extern Titus_Mutex RepMutex;
extern Titus_TimedMutex QueueMutex;
extern AD_CONFIG ad_config;
extern READER readers[];
extern FEEDER feeders[];
extern QUEUE *pQue; // main play queue
extern QUEUE *rQue; // request queue
typedef std::vector<QUEUE *> promoQueue;
extern promoQueue promos;
//extern QUEUE *prFirst,*prLast;
//extern QUEUE *prCur;
typedef std::map<string, int64, ci_less> ArtistListType;
typedef std::map<uint32, int64> RequestListType;
typedef std::map<string, int64, ci_less> RequestUserListType;
extern ArtistListType artistList;
extern RequestListType requestList;
extern RequestUserListType requestUserList;
extern RequestUserListType requestArtistList;

void QueueContentFiles(bool promos_too=false);
void QueuePromoFiles(const char *path);
void AddToQueue(QUEUE * q, QUEUE ** qqFirst, QUEUE ** qqLast);
int ReadMetaDataEx(const char * fn, TITLE_DATA * td, uint32 * songlen=NULL, DECODER * pDec=NULL);
uint32 FileID(const char * fn);
unsigned long QM_CRC32(unsigned char *block, unsigned int length);
int32 DoFileScan(FSProcFileType proc, FSPostProc post_proc, void * uptr);

READER_HANDLE * OpenFile(const char * fn, int32 mode);
bool ShouldIStop();
DECODER * GetFileDecoder(const char * fn);
QUEUE * GetNextSong();
void AddToPlayQueue(QUEUE * song);
bool AllowRequest(QUEUE * Scan, USER_PRESENCE * ut);
int AddRequest(QUEUE * Scan, const char * nick, bool to_front=false);

sql_rows GetTable(char * query, char **errmsg);
void RateSong(QUEUE * song, const char * nick, uint32 rating);
void GetSongRating(uint32 FileID, SONG_RATING * r);

const char * GetDeckName(AUTODJ_DECKS deck);
bool MatchesNoPlayFilter(const char * str);
bool MatchFilter(const QUEUE * Scan, TIMER * Timer, const char * pat);
char * PrettyTime(int32 seconds, char * buf);
Audio_Buffer * AllocAudioBuffer();
void FreeAudioBuffer(Audio_Buffer * buf);

//TITLE_DATA * AllocTitleData();
void QueueTitleUpdate(TITLE_DATA * td);
char * urlencode(char * buf, unsigned long bufSize);
char * urlencode(const char * in, char * out, unsigned long bufSize);

// in timing.cpp
void Timing_Reset(int32 SongLen);
void Timing_Add(int32 iTime);
void Timing_Done();
bool Timing_IsDone();

int32 GetTimeLeft(bool secs=false);
void timer_init(AUTODJ_TIMER * t, uint64 denominator);
void timer_set(AUTODJ_TIMER * t, uint64 whole, uint64 numerator);
void timer_add(AUTODJ_TIMER * t, uint64 val);
void timer_zero(AUTODJ_TIMER * t);
uint64 timer_value(AUTODJ_TIMER * t);
uint64 timer_whole_value(AUTODJ_TIMER * t);

// in plugins.cpp
AD_PLUGIN_API * GetAPI();
void LoadPlugins(const char * path, bool quiet=false);
void UnloadPlugins();
bool LoadQueuePlugin(const char * fn);
void UnloadQueuePlugin();
bool LoadRotationPlugin(const char * fn);
void UnloadRotationPlugin();
bool LoadResamplerPlugin(const char * fn);
void UnloadResamplerPlugin();


// in schedule.cpp
THREADTYPE ScheduleThread(void * lpData);
void LoadSchedule();
void SchedulerProcText(char * text, time_t tme, unsigned int bufSize);

// in mixer.cpp
MIXER * GetMixer();
void MixerShutdown();
bool MixerStartup();
void UnloadActiveDeck();

// in req_iface.cpp
extern REQUEST_INTERFACE adj_req_iface;

// in deck.cpp
bool MoveFromQueue(const char * fn);

// in resampler.cpp
#include "./resample/resample.h"
class ResamplerSpeex: public Resampler {
private:
	SpeexResamplerState * ss;
public:
	ResamplerSpeex() { ss = NULL; }
	virtual ~ResamplerSpeex() { Close(); }
	virtual bool Init(AUTODJ_DECKS deck, int32 numChannels, int32 inSampleRate, int32 outSampleRate);
	virtual int32 Resample(int32 samples, Audio_Buffer * bufIn, Audio_Buffer * bufOut);
	virtual void Close();
};

// in state.cpp
void LoadState();
void SaveState();

#endif // __AUTODJ_H__

