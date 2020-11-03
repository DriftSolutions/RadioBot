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


#include "../adj_plugin.h"
#include "NSV_Reader.h"

/*
struct buffer {
  unsigned char const *start;
  unsigned long length;
};
buffer buf2;
*/

SD_PLUGIN_API * api=NULL;

bool nsv_is_my_file(const char * fn, const char * mime_type) {
	if ( strrchr(fn,'.') && !stricmp(strrchr(fn,'.'),".nsv")) {
		return true;
	}
	return false;
}

bool nsv_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	return false;
}

class NSV_Decoder: public SimpleDJ_Decoder {
private:
	//buffer mbuf;
	//uint8 frame[MP3_MAX_FRAME_SIZE];

	/*
	int channels;
	int samplerate;
	READER_HANDLE * fp;

	int samples_done;
	time_t start_time;
	int song_time;
	*/
	NSV_Reader * reader;
	READER_HANDLE * nsv_file;
	long nsv_stime;
	unsigned long nsv_fcount;

public:
	virtual ~NSV_Decoder() {

	}

	virtual bool Open(READER_HANDLE * fnIn, TITLE_DATA * meta, int64 startpos) {
		nsv_file = NULL;
		reader = new NSV_Reader();
		char buf[1024];
		buf[0] = 0;
		if (!reader->Open(fnIn, buf, sizeof(buf))) {
			api->botapi->ib_printf("AutoDJ (nsv_decoder) -> Error opening NSV file: %s!\n", buf);
			delete reader;
			return false;
		}

		nsv_file = fnIn;
		nsv_stime=0;
		nsv_fcount=0;

		NSV_File * file = reader->GetFileInformation();
		if (!file) {
			delete reader;
			reader=NULL;
			api->botapi->ib_printf("AutoDJ -> Not a valid NSV file!\n");
			nsv_file=NULL;
			return false;
		}

		api->botapi->ib_printf("AutoDJ (nsv_decoder) -> NSV Opened. (Has Header: %s)\n",file->header.nsv_sig != 0 ? "Yes":"No");
		if (file->header.nsv_sig != 0) {
			api->botapi->ib_printf("AutoDJ (nsv_decoder) -> Has MetaData: %s, Has TOC 1.0: %s, Has TOC 2.0: %s\n",file->metadata != NULL ? "Yes":"No",file->toc != NULL ? "Yes":"No",(file->header.toc_alloc > (file->header.toc_size * 2)) ? "Yes":"No");
		}

		api->botapi->ib_printf("AutoDJ (nsv_decoder) -> Codec Info -> VideoFmt: '%s', AudioFmt: '%s'\n",file->vidfmt_str,file->audfmt_str);
		api->botapi->ib_printf("AutoDJ (nsv_decoder) -> Other Info -> Size: %ux%u, FR: %f\n",file->info.width,file->info.height,file->framerate);

		if (file->header.nsv_sig != 0 && file->header.file_len_ms != 0 && file->header.file_len_ms != 0xFFFFFFFF) {
			api->Timing_Reset(file->header.file_len_ms);
		} else {
			api->Timing_Reset(0);
		}

		nsv_stime=time(NULL);
		nsv_fcount=0;

		TITLE_DATA *td = (TITLE_DATA *)malloc(sizeof(TITLE_DATA));
		memset(td,0,sizeof(TITLE_DATA));

		if (!strlen(td->title)) {
			td->artist[0]=0;
			if (!strcmp(fnIn->type,"file")) {
				#ifdef _WIN32
				if (strrchr(fnIn->fn,'\\')) {
					strcpy(td->title,strrchr(fnIn->fn,'\\')+1);
				} else {
				#else
				if (strrchr(fnIn->fn,'/')) {
					strcpy(td->title,strrchr(fnIn->fn,'/')+1);
				} else {
				#endif
					strcpy(td->title,fnIn->fn);
				}
			} else {
				strcpy(td->title,fnIn->fn);
			}
		}

		str_replace(td->title,sizeof(td->title),".nsv","");
		str_replace(td->title,sizeof(td->title),"."," ");
		strtrim(td->title," ");
		strtrim(td->artist," ");

		api->QueueTitleUpdate(td);
		free(td);

		return true;//api->GetEncoder()->init(0,0);
	}

	virtual bool Open_URL(const char * url, TITLE_DATA * meta, int64 startpos) {
		return false;
	}

	virtual int64 GetPosition() {
		return (nsv_file != NULL) ? nsv_file->tell(nsv_file):0;
	}

	virtual DECODE_RETURN Decode() {
		NSV_Buffer buf;
		buf.data = new unsigned char[MAX_FRAME_SIZE];
		buf.len = MAX_FRAME_SIZE;
		buf.ind = 0;
		DECODE_RETURN ret = SD_DECODE_DONE;
		while (reader->PopFrame(&buf)) {
			if (api->ShouldIStop()) {
				break;
			}
			if (!api->GetMixer()->raw(buf.data,buf.ind)) {
				ret = SD_DECODE_ERROR;
				break;
			}
			reader->ClearBuffer(&buf);

			nsv_fcount++;

			double target = reader->GetFileInformation()->framerate * (time(NULL) - nsv_stime);
			while (nsv_fcount > target) {
				api->botapi->safe_sleep_milli(25);
				target = reader->GetFileInformation()->framerate * (time(NULL) - nsv_stime);
			}
		}
		delete buf.data;
		return ret;
	}

	virtual void Close() {
		api->botapi->ib_printf("AutoDJ (nsv_decoder) -> nsv_close()\n");
		if (reader) {
			delete reader;
			reader=NULL;
		}
		nsv_file = NULL;
		api->Timing_Done();
	}
};

SimpleDJ_Decoder * nsv_create() { return new NSV_Decoder; }
void nsv_destroy(SimpleDJ_Decoder * dec) { delete dynamic_cast<NSV_Decoder *>(dec); }

DECODER nsv_decoder = {
	nsv_is_my_file,
	nsv_get_title_data,
	nsv_create,
	nsv_destroy
};

bool init(SD_PLUGIN_API * pApi) {
	api = pApi;
	api->RegisterDecoder(&nsv_decoder);
	return true;
};

void quit() {
};

SD_PLUGIN plugin = {
	SD_PLUGIN_VERSION,
	"Nullsoft Streaming Video (NSV) Decoder",

	init,
	NULL,
	quit
};

PLUGIN_EXPORT SD_PLUGIN * GetSDPlugin() { return &plugin; }

