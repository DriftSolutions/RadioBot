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


#ifndef _SDJ_PLUGIN_H_
#define _SDJ_PLUGIN_H_

enum DECODE_RETURN {
	SD_DECODE_DONE			= 0x00,
	SD_DECODE_CONTINUE		= 0x01,
	SD_DECODE_ERROR			= 0x02
};

#if defined(_SIMPLEDJ_) || !defined(_WIN32)
#include "../../src/plugins.h"
#else
#include "../../../src/plugins.h"
#endif

#include "scheduler.h"
//#include "dsp.h"

#include <drift/dsl.h>

struct TITLE_DATA {
	char artist[256];
	char album[256];
	char title[256];
	char genre[128];
};

struct REQ_INFO {
	int32 netno;
	char channel[64];
	char nick[128];
};

struct QUEUE {
	uint32 ID;
	char * fn;
	char * path;
	int64 LastPlayed, mTime;
	uint32 songlen;
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
	char * buf;
	int32 len;
};

struct AUDIO_INFO {
	int32 channels;
	int32 sample;
};

struct READER_HANDLE {
	int32 (*seek)(READER_HANDLE * fp, int32 offset, int32 mode);
	int32 (*tell)(READER_HANDLE * fp);
	int32 (*read)(void * buf, int32 size, READER_HANDLE * fp);
	int32 (*write)(void * buf, int32 objSize, int32 count, READER_HANDLE * fp);
	bool (*eof)(READER_HANDLE * fp);
	void (*close)(READER_HANDLE * fp);

	char * fn;
	char type[16]; // should be "file" or "www"
	char mime_type[32];

	bool can_seek, is_eof;
	uint32 fileSize;

	union {
		void * data;
		FILE * fp;
	};

	char * desc;
};

struct READER {
	bool (*is_my_file)(const char * fn);
	READER_HANDLE * (*open)(const char * fn, const char * mode);
};

class SimpleDJ_Decoder {
	public:
		READER_HANDLE * fp;
		virtual bool Open(READER_HANDLE * fpp, TITLE_DATA * meta, int64 startpos)=0;
		virtual bool Open_URL(const char * url, TITLE_DATA * meta, int64 startpos)=0;
		virtual int64 GetPosition()=0;
		//virtual int Decode(short samples[], int max_samples)=0;
		virtual DECODE_RETURN Decode()=0;
		virtual void Close()=0;
};

struct DECODER {
	bool (*is_my_file)(const char * fn, const char * mime_type);
	bool (*get_title_data)(const char * fn, TITLE_DATA * td, uint32 * songlen);

	SimpleDJ_Decoder * (*create)();
	void (*destroy)(SimpleDJ_Decoder * dec);
};

/*
class SimpleDJ_Encoder {
public:
	virtual bool Init(int32 num, int32 channels, int32 samplerate, float scale)=0;
	virtual bool Encode(int32 num_samples, const short buffer[])=0;
	virtual bool Raw(unsigned char * data, int32 len)=0;
	virtual void Close()=0;
};

struct ENCODER {
	char name[32];
	char * mimetype;

	SimpleDJ_Encoder * (*create)();
	void (*destroy)(SimpleDJ_Encoder * dec);
};
*/

struct MIXER {
	//bool (*init)(int32 channels, int32 samplerate);
	//void (*close)();
	bool (*raw)(unsigned char * data, int32 len);

	//int32 inChannels, inSampleRate;
	//bool lastDSPinit;
};

class SimpleDJ_Feeder {
protected:
	int num;
	API_SS ss;
public:
	SimpleDJ_Feeder();
	virtual ~SimpleDJ_Feeder();
	virtual bool Connect()=0;
	virtual bool IsConnected()=0;
	virtual bool Send(unsigned char * buffer, int32 len)=0;
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td)=0;
	virtual void Close()=0;

	void SetNum(int ss_num) { num = ss_num; }
};

struct FEEDER {
	SimpleDJ_Feeder * (*create)();
	void (*destroy)(SimpleDJ_Feeder * feeder);
	char type;
	char * desc;
};

struct SD_CONFIG_FILELISTER {
	char List[256];
	char Line[256];
	char Header[256];
	char Footer[256];
	char NewChar[256];
};

#if defined(_SIMPLEDJ_)
#define SimpleDJ_Rand api->genrand_int32
#else
#define SimpleDJ_Rand api->botapi->genrand_int32
#endif

struct SD_PLUGIN_API {
	BOTAPI_DEF * botapi;
	int (*GetPluginNum)();
	void (*QueueTitleUpdate)(TITLE_DATA * ptr);
	bool (*ShouldIStop)();
	MIXER   * (*GetMixer)();
	//ENCODER * (*GetEncoder)();
	//void (*RegisterEncoder)(ENCODER * Encoder);
	void (*RegisterDecoder)(DECODER * dec);

	void (*Timing_Reset)(int32 SongLen);
	void (*Timing_Add)(int32 iTime);
	void (*Timing_Done)();

	int32 (*GetOutputChannels)(int num);
	int32 (*GetOutputSample)(int num);
	int32 (*GetOutputBitrate)(int num);

	SimpleDJ_Feeder * (*GetFeeder)(int num);

	DL_HANDLE (*getInstance)();

	//new stuff for queue plugins

	bool (*MatchFilter)(QUEUE * Scan, TIMER * Timer, char * pat);
	bool (*MatchesNoPlayFilter)(char * str);
	void (*ProcText)(char * text, time_t tme, unsigned int bufSize);
	char * (*PrettyTime)(int32 seconds, char * buf);
	SD_CONFIG_FILELISTER * FileLister;
	DECODER * (*GetFileDecoder)(const char * fn);
	char * (*getContentDir)();
	char * (*getNoPlayFilters)();
	TIMER * (*getActiveFilter)();
	void (*removeActiveFilter)();
	uint32 (*getNoRepeat)();
	void (*setNoRepeat)(uint32 nr);
	QUEUE * (*CopyQueue)(QUEUE * Scan);
	//QUEUE * (*AllocQueue)();
	void * (*malloc)(size_t sz);
	char * (*strdup)(const char * str);
	void (*free)(void * ptr);
	void (*FreeQueue)(QUEUE * Scan);
	Titus_Mutex * (*getQueueMutex)();
};

#define SD_PLUGIN_VERSION 0x02

struct SD_PLUGIN {
	unsigned char version;
	char * plugin_desc;

	bool (*init)(SD_PLUGIN_API * pApi);
	int (*message)(unsigned int msg, char * data, long datalen);//recieves copies of ircbot messages
	void (*quit)();

	DL_HANDLE hModule;
};

enum META_TYPES {
	META_FILENAME,
	META_DIRECTORY,
	META_TITLE,
	META_ARTIST,
	META_ALBUM,
	META_GENRE
};

struct SD_QUEUE_PLUGIN {
	unsigned char version;
	char * plugin_desc;

	bool (*init)(SD_PLUGIN_API * pApi);
	void (*quit)();
	void (*on_song_play)(QUEUE * song);
	void (*on_song_play_over)(QUEUE * song);
	void (*on_song_request)(QUEUE * song);
	void (*on_song_rating)(uint32 song, uint32 rating, uint32 votes);

	int32 (*QueueContentFiles)();
	void (*FreePlayList)();

	QUEUE_FIND_RESULT * (*FindWild)(const char * pat);
	QUEUE_FIND_RESULT * (*FindByMeta)(const char * pat, META_TYPES type);
	void (*FreeFindResult)(QUEUE_FIND_RESULT * qRes);

	QUEUE * (*FindFile)(const char * fn);
	QUEUE * (*FindFileByID)(uint32 id);
	QUEUE * (*GetRandomFile)();

	DL_HANDLE hModule;
};

#endif // _SDJ_PLUGIN_H_
