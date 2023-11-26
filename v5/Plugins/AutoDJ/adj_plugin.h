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


#ifndef _ADJ_PLUGIN_H_
#define _ADJ_PLUGIN_H_

enum DECODE_RETURN {
	AD_DECODE_DONE			= 0x00,
	AD_DECODE_CONTINUE		= 0x01,
	AD_DECODE_ERROR			= 0x02
};

enum AUTODJ_DECKS {
	DECK_A,
	DECK_B,
	NUM_DECKS
};

#if defined(_AUTODJ_) || !defined(_WIN32)
#include "../../src/plugins.h"
#else
#include "../../../src/plugins.h"
#endif

#include "scheduler.h"
#include "dsp.h"

struct TITLE_DATA {
	char artist[256];
	char album[256];
	char title[256];
	char album_artist[256];
	char genre[128];
	char url[256];
	char comment[256];
	uint32 track_no;
	uint32 year;
};

struct REQ_INFO {
	int32 netno;
	char channel[64];
	char nick[128];
	time_t playAfter;
};

#define QUEUE_FLAG_NO_TITLE_UPDATE	0x0001
#define QUEUE_FLAG_DELETE_AFTER		0x0002

struct QUEUE {
	uint32 ID;
	char * fn;
	char * path;
	int64 lastPlayed, mTime;
	uint32 songlen;
	int32 req_count;
	uint16 flags;
	TITLE_DATA * meta;
	REQ_INFO * isreq;
	QUEUE *Prev,*Next;
};

#define MAX_RESULTS 256
struct QUEUE_FIND_RESULT {
	int32 num;
	bool not_enough;
	QUEUE * results[MAX_RESULTS];
};

struct Audio_Buffer {
	int16 * buf;
	int32 len;

	void (*realloc)(Audio_Buffer * buf, size_t nSize);
	void (*reset)(Audio_Buffer * buf);
};

struct AUDIO_INFO {
	int32 channels;
	int32 sample;
};

class Resampler {
protected:
	AUTODJ_DECKS deck;
	int32 numChannels, inSampleRate, outSampleRate;
public:
	virtual ~Resampler() {};
	virtual bool Init(AUTODJ_DECKS deck, int32 numChannels, int32 inSampleRate, int32 outSampleRate) = 0;
	virtual int32 Resample(int32 samples, Audio_Buffer * bufIn, Audio_Buffer * bufOut) = 0;
	virtual void Close() = 0;
};

struct READER_HANDLE {
	int64 (*seek)(READER_HANDLE * fp, int64 offset, int32 mode);
	int64 (*tell)(READER_HANDLE * fp);
	int32 (*read)(void * buf, int32 size, READER_HANDLE * fp);
	int32 (*write)(void * buf, int32 size, READER_HANDLE * fp);
	bool (*eof)(READER_HANDLE * fp);
	void (*close)(READER_HANDLE * fp);

	char * fn;
	char type[16]; // should be "file" or "www"
	char mime_type[32];

	bool can_seek, is_eof;
	int64 fileSize;

	union {
		void * data;
		FILE * fp;
		int fd;
	};

	char * desc;
};

#define READER_RDONLY	0x01
#define READER_WRONLY	0x02
#define READER_RDWR		0x03
#define READER_TRUNC	0x04
#define READER_CREATE	0x08

struct READER {
	bool (*is_my_file)(const char * fn);
	READER_HANDLE * (*open)(const char * fn, int32 mode);
};

class AutoDJ_Decoder {
protected:
	AUTODJ_DECKS deck;
	int32 song_length;
public:
	READER_HANDLE * fp;
	virtual bool Open(READER_HANDLE * fpp, int64 startpos)=0;
	virtual bool Open_URL(const char * url, int64 startpos)=0;
	virtual DECODE_RETURN Decode()=0;
	virtual void Close()=0;
	virtual ~AutoDJ_Decoder() {};

	virtual int64 GetPosition()=0;

	virtual bool WantTitleUpdate() { return true; }
	int32 GetLength() { return song_length; }
	void SetLength(int32 len) { song_length = len; }
	void SetDeck(AUTODJ_DECKS ideck) { deck = ideck; }
	AUTODJ_DECKS GetDeck() { return deck; }
};

struct DECODER {
	int32 priority;

	bool (*is_my_file)(const char * fn, const char * mime_type);
	bool (*get_title_data)(const char * fn, TITLE_DATA * td, uint32 * songlen);

	AutoDJ_Decoder * (*create)();
	void (*destroy)(AutoDJ_Decoder * dec);
};

struct AUTODJ_TIMER {
	int64 whole;
	int32 num;
	int32 den;
};

class Deck {
private:
	AUTODJ_DECKS deck;
	Resampler * resampler;
	AutoDJ_Decoder * dec;
	Audio_Buffer * abuffer;
	DECODER * selDec;
	READER_HANDLE * fp;
	QUEUE * Queue;

	bool paused, firstPlay, didTimingReset, move;
	TITLE_DATA meta;
	AUTODJ_TIMER timer;
	uint64 lastWhole;
	int32 inChannels;
public:
	Deck(AUTODJ_DECKS deck);
	virtual ~Deck();

	bool LoadFile(const char * fn, QUEUE * Scan=NULL, TITLE_DATA * meta=NULL);
	bool Play();
	void Pause() { paused = true; }
	void SetMove() { move = true; }
	void Stop();

	bool isLoaded() { return (dec != NULL) ? true:false; }
	bool isPlaying() { return (dec != NULL && !paused) ? true:false; }
	bool isPaused() { return paused; }

	virtual int32 AddSamples(Audio_Buffer * buffer);
	virtual int32 AddSamples(short * buffer, int32 samples);
	virtual bool SetInAudioParams(int32 channels, int32 samplerate);

	void Decode(int32 num);
	int32 GetSamples(Audio_Buffer * buffer, int32 num);
	int32 GetSamples(short * buffer, int32 samples);
	int32 GetBufferedSamples();
	int64 getLength() { return dec->GetLength(); }
	int64 getOutputTime();
	QUEUE * getSong() { return Queue; }
};

class AutoDJ_Encoder {
public:
	virtual ~AutoDJ_Encoder() {}
	virtual bool Init(int32 channels, int32 samplerate, float scale)=0;
	virtual bool Encode(int32 num_samples, const short buffer[])=0;
	virtual bool Raw(unsigned char * data, int32 len)=0;
	virtual void Close()=0;
	virtual void OnTitleUpdate(const char * title, TITLE_DATA * td) {};
};

struct ENCODER {
	char name[32];
	char * mimetype;
	int32 priority;

	AutoDJ_Encoder * (*create)();
	void (*destroy)(AutoDJ_Encoder * dec);
};

struct MIXER {
	bool (*init)();
	void (*close)();

	//bool (*encode_signed_int)(int32 samples, const int left[], const int right[]);
	//bool (*encode_short)(int32 samples, short left[], short right[]);
	bool (*encode)();
	bool (*raw)(unsigned char * data, int32 len);
	uint64 (*getOutputTime)();

	//int32 inChannels, inSampleRate;
	//Audio_Buffer * abuffers[NUM_DECKS];
	//bool lastDSPinit;
	AUTODJ_DECKS curDeck, othDeck;
	AUTODJ_TIMER timer;
};

class AutoDJ_Feeder {
public:
	AutoDJ_Feeder();
	virtual ~AutoDJ_Feeder();
	virtual bool Connect()=0;
	virtual bool IsConnected()=0;
	virtual bool Send(unsigned char * buffer, int32 len)=0;
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td)=0;
	virtual void Close()=0;
};

struct FEEDER {
	AutoDJ_Feeder * (*create)();
	void (*destroy)(AutoDJ_Feeder * feeder);
	char type;
	char * desc;
};

struct AD_CONFIG_FILELISTER {
	char List[256];
	char Line[256];
	char Header[256];
	char Footer[256];
	char NewChar[256];
};

#if defined(_AUTODJ_)
#define AutoDJ_Rand api->genrand_int32
#else
#define AutoDJ_Rand adapi->botapi->genrand_int32
#endif

#define MAX_PLAYLISTS 32
typedef bool (*FSProcFileType)(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, int playlist, void * uptr);//, struct stat st
typedef bool (*FSPostProc)(READER_HANDLE * fOutList);

struct AD_PLUGIN_API {
	BOTAPI * botapi;
	int32 (*GetPluginNum)();

	void (*QueueTitleUpdate)(TITLE_DATA * ptr);
	void (*RegisterEncoder)(ENCODER * Encoder);
	void (*RegisterDecoder)(DECODER * dec);

	void (*Timing_Reset)(int32 SongLen);
	void (*Timing_Add)(int32 iTime);
	void (*Timing_Done)();

	int32 (*GetOutputChannels)();
	int32 (*GetOutputSample)();
	int32 (*GetOutputBitrate)();

	AutoDJ_Feeder * (*GetFeeder)();
	//Resampler * (*AllocResampler)();
	//void (*FreeResampler)(Resampler * r);
	Deck * (*GetDeck)(AUTODJ_DECKS deck);
	const char * (*GetDeckName)(AUTODJ_DECKS deck);

	DL_HANDLE (*getInstance)();

	uint32 (*FileID)(const char * fn);
	int32 (*DoFileScan)(FSProcFileType proc, FSPostProc post_proc, void * uptr);
	bool (*getOnlyScanNewFiles)();

	bool (*AllowSongPlayback)(const QUEUE * Scan, time_t tme, const char * pat);
	void (*AddToPlayQueue)(QUEUE * song);

	bool (*MatchFilter)(const QUEUE * Scan, TIMER * Timer, const char * pat);
	bool (*MatchesNoPlayFilter)(const char * str);
	void (*ProcText)(char * text, time_t tme, uint32 bufSize);
	char * (*PrettyTime)(int32 seconds, char * buf);
	AD_CONFIG_FILELISTER * FileLister;
	DECODER * (*GetFileDecoder)(const char * fn);
	int (*ReadMetaDataEx)(const char * fn, TITLE_DATA * td, uint32 * songlen, DECODER * pDec);
	//char * (*getContentDir)();
	int32 (*getNumPlaylists)();
	int32 (*getID3Mode)();
	char * (*getNoPlayFilters)();
	TIMER * (*getActiveFilter)();
	void (*removeActiveFilter)();
	uint32 (*getNoRepeat)();
	void (*setNoRepeat)(uint32 nr);
	uint32 (*getNoRepeatArtist)();
	void (*setNoRepeatArtist)(uint32 nr);
	uint32 (*getMaxSongDuration)();
	void (*setMaxSongDuration)(uint32 nr);
	QUEUE * (*AllocQueue)();
	QUEUE * (*CopyQueue)(const QUEUE * Scan);
	void (*FreeQueue)(QUEUE * Scan);
	void * (*malloc)(size_t sz);
	char * (*strdup)(const char * str);
	void (*free)(void * ptr);
	Audio_Buffer * (*AllocAudioBuffer)();
	void (*FreeAudioBuffer)(Audio_Buffer * buf);
	Titus_Mutex * (*getQueueMutex)();
};
#define ReadMetaData(x,y) ReadMetaDataEx(x,y,NULL,NULL)

#define AD_PLUGIN_VERSION IRCBOT_PLUGIN_VERSION+0x11

struct AD_PLUGIN {
	unsigned char version;
	char * plugin_desc;

	bool (*init)(AD_PLUGIN_API * pApi);
	int32 (*message)(uint32 msg, char * data, int32 datalen);//recieves copies of ircbot messages
	void (*quit)();

	DL_HANDLE hModule;
};

enum META_TYPES {
	META_FILENAME,
	META_DIRECTORY,
	META_TITLE,
	META_ARTIST,
	META_ALBUM,
	META_GENRE,
	META_YEAR,
	META_REQ_COUNT, /// FindByMeta extra: 0 = file that has never been requested, > 0 file that has been requested at least X times, < 0 TBD
};

struct AD_QUEUE_PLUGIN {
	unsigned char version;
	const char * plugin_desc;

	bool (*init)(AD_PLUGIN_API * pApi);
	void (*quit)();
	void (*on_song_play)(QUEUE * song);
	void (*on_song_play_over)(QUEUE * song);
	void (*on_song_request)(QUEUE * song, const char * nick);
	void (*on_song_request_delete)(QUEUE * song);
	void (*on_song_rating)(uint32 song, uint32 rating, uint32 votes);

	int32 (*QueueContentFiles)();
	void (*FreePlayList)();

	QUEUE_FIND_RESULT * (*FindWild)(const char * pat);
	QUEUE_FIND_RESULT * (*FindByMeta)(const char * pat, META_TYPES type);
	void (*FreeFindResult)(QUEUE_FIND_RESULT * qRes);

	QUEUE * (*FindFile)(const char * fn);
	QUEUE * (*FindFileByID)(uint32 id);
	QUEUE * (*GetRandomFile)(int playlist);

	DL_HANDLE hModule;
};

struct AD_ROTATION_PLUGIN {
	unsigned char version;
	const char * plugin_desc;

	bool (*init)(AD_PLUGIN_API * pApi);
	void (*quit)();

	int32 (*message)(uint32 msg, char * data, int32 datalen);//recieves copies of ircbot messages
	void (*on_song_play)(QUEUE * song);
	void (*on_need_songs)();

	DL_HANDLE hModule;
};

struct AD_RESAMPLER_PLUGIN {
	unsigned char version;
	const char * plugin_desc;

	bool (*init)(AD_PLUGIN_API * pApi);
	void (*quit)();

	Resampler * (*create)();
	void (*destroy)(Resampler *);

	DL_HANDLE hModule;
};

#endif // _ADJ_PLUGIN_H_
