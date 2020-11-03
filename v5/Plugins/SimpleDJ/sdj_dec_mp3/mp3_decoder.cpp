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


#include "mp3_decoder.h"

SD_PLUGIN_API * api=NULL;
Titus_Mutex tagMutex;

bool mp3_is_my_file(const char * fn, const char * mime_type) {
	if (fn) {
		const char * ext = strrchr(fn,'.');
		if (ext && (!stricmp(ext,".mp3") || !stricmp(ext,".cm3")))   {
			return true;
		}
	}
	//if (fn && strrchr(fn,'.') && ( !stricmp(strrchr(fn,'.'),".mp3") || !stricmp(strrchr(fn,'.'),".cm3") ) ) {
	//	return true;
	//}
	if (mime_type && !stricmp(mime_type, "audio/mpeg")) { return true; }
	return false;
}

bool IsNum(char * p) {
	size_t len = strlen(p);
	for (size_t i=0; i < len; i++) {
		if (p[i] < '0' || p[i] > '9') {
			return false;
		}
	}
	return true;
}

bool mp3_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	tagMutex.Lock();
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }
	//uint32 ticks = GetTickCount();
	bool ret = false;

#if defined(USE_TAGLIB)
	TagLib::FileRef iTag(fn);
	if (!iTag.isNull() && iTag.tag() && !iTag.tag()->isEmpty()) {
		TagLib::String p = iTag.tag()->title();
		if (!p.isNull()) {
			strlcpy(td->title,p.toCString(true),sizeof(td->title));
			ret = true;
		}
		p = iTag.tag()->artist();
		if (!p.isNull()) {
			strlcpy(td->artist,p.toCString(true),sizeof(td->artist));
		}
		p = iTag.tag()->album();
		if (!p.isNull()) {
			strlcpy(td->album,p.toCString(true),sizeof(td->album));
		}
		p = iTag.tag()->genre();
		if (!p.isNull()) {
			strlcpy(td->genre,p.toCString(true),sizeof(td->genre));
		}
	}
#endif

	//ticks = GetTickCount() - ticks;
	//api->botapi->ib_printf("SimpleDJ (mp3_decoder) -> Spent %u ticks scanning: %s\n", ticks, fn);
	tagMutex.Release();
	return ret;
}

#define MPEG_1 3
#define MPEG_2 2
#define MPEG_25 0

static const int mpeg_bitrates[2][16] = {
	{ 0,32,40,48,56,64,80,96,112,128,160,192,224,256,320},
	{ 0, 8,16,24,32,40,48,56, 64, 80, 96,112,128,144,160}
};

static const long mpeg_samplerates[3][3] = {
	{ 44100, 48000, 32000 },
	{ 22050, 24000, 16000 },
	{ 11025, 12000, 8000 }
};

enum MP3_RETURN {
	MP3_OK,
	MP3_DONE,
	MP3_ERROR
};

struct buffer {
	uint8 * data;
	int32 len;
	int64 filepos;
};

int read_mp3_data(READER_HANDLE * fp, buffer * buf) {
	if (fp->eof(fp)) { return 0; }
	int bytes_read = fp->read(buf->data + buf->len, MP3_BUFFER_SIZE-buf->len, fp);
	int tot_read = 0;
	while (!fp->eof(fp) && bytes_read > 0 && buf->len < MP3_BUFFER_SIZE) {
		buf->len += bytes_read;
		tot_read += bytes_read;
		bytes_read = fp->read(buf->data + buf->len, MP3_BUFFER_SIZE-buf->len, fp);
	}
	if (tot_read) { return tot_read; }
	return bytes_read;
}

struct MP3_HEADER {
	int version;
	int layer;
	bool protection;
	int bitrate;
	int samplerate;
	bool padding;
	bool is_private;
	int channels;
	int mode_ext;
	bool copyright;
	bool original;
	int emphasis;
};

#define header_cmp 0xfffe0c00
MP3_RETURN read_mp3_frame(READER_HANDLE * fp, buffer * buf, MP3_HEADER * h, uint8 * frame, int * len, uint32 * last_hdr) {
	MP3_RETURN ret = MP3_DONE;
	//bool done = false;
	while (1) {
		if (buf->len < (MP3_BUFFER_SIZE/2)) {
			if (read_mp3_data(fp, buf) == -1) {
				ret = MP3_ERROR;
				break;
			}
		}
		if (buf->len <= 4) {
			break;
		}
		uint32 head = Get_UBE32(*((uint32 *)buf->data));
		/*          sync word                      mpeg version 1/2/2.5     make sure layer 3          check invalid bitrate       check invalid samplerate */
		if (((head & 0xffe00000) != 0xffe00000) || (((head>>19)&3) == 1) || (((head>>17)&3) != 1) || (((head>>12)&0xf) == 0xf) || (((head>>12)&0xf) == 0) || (((head>>10)&0x3) == 0x3)) {
			if (head == SSMT_MAGIC) {
				uint32 tlen = LibSSMT_UnsynchInt(Get_UBE32(*((uint32*)(buf->data+4))));
				memmove(buf->data, buf->data + 8, buf->len - 8);
				buf->len -= 8;
				buf->filepos += 8;
				if (read_mp3_data(fp, buf) == -1) {
					ret = MP3_ERROR;
					break;
				}
				if ((int32)tlen <= buf->len) {
					SSMT_TAG * ctx_tag = LibSSMT_NewTag();
					LibSSMT_ParseTagBuffer(ctx_tag, buf->data, tlen);
					api->botapi->ib_printf(_("SimpleDJ -> SSMT tag @ " I64FMT ": %d items!\n"), buf->filepos-8, ctx_tag->num_items);
					TITLE_DATA td;
					memset(&td,0,sizeof(td));
					//strcpy(td.title,ctx_tag->title);
					SSMT_ITEM * it = LibSSMT_GetItem(ctx_tag, SSMT_TITL);
					if (it != NULL) {
						strlcpy(td.title, it->str, sizeof(td.title));
						it = LibSSMT_GetItem(ctx_tag, SSMT_ARTS);
						if (it) {
							strlcpy(td.artist, it->str, sizeof(td.artist));
						}
						it = LibSSMT_GetItem(ctx_tag, SSMT_ALBM);
						if (it) {
							strlcpy(td.album, it->str, sizeof(td.album));
						}
						it = LibSSMT_GetItem(ctx_tag, SSMT_GNRE);
						if (it) {
							strlcpy(td.genre, it->str, sizeof(td.genre));
						}
						api->QueueTitleUpdate(&td);
					} else {
						if ((it = LibSSMT_GetItem(ctx_tag, SSMT_STIT)) != NULL) {
							strlcpy(td.title, it->str, sizeof(td.title));
							api->QueueTitleUpdate(&td);
						}
					}
					LibSSMT_FreeTag(ctx_tag);
					if ((buf->len - tlen) > 0) {
						memmove(buf->data, buf->data + tlen, buf->len - tlen);
					}
					buf->len -= tlen;
					buf->filepos += tlen;
				} else {
					api->botapi->ib_printf(_("SimpleDJ -> WARNING: SSMT tag @ " I64FMT " is too large (or has been truncated)!\n"), buf->filepos-8);
				}
			} else {
				//api->botapi->ib_printf(_("SimpleDJ -> WARNING: Junk @ "I64FMT": %02X!\n"), buf->filepos, *buf->data);
				memmove(buf->data, buf->data + 1, buf->len - 1);
				buf->len--;
				buf->filepos++;
			}
			continue;
		}
		if (*last_hdr && (head & header_cmp) != (*last_hdr & header_cmp)) {
			api->botapi->ib_printf(_("SimpleDJ -> WARNING: MP3 header @ " I64FMT " differed substantially from last frame, file may be corrupt!\n"), buf->filepos);
			//memmove(buf->data, buf->data + 1, buf->len - 1);
			//buf->len--;
			//buf->filepos++;
			//continue;
		}
		*last_hdr = head;
		h->version = (head>>19)&0x03;
		h->layer = 3;
		h->protection = ((head>>16)&0x01) ? true:false;
		if (h->version == MPEG_1) {
			h->bitrate = mpeg_bitrates[0][(head>>12)&0x0F];
			h->samplerate = mpeg_samplerates[0][(head>>10)&0x03];
		} else if (h->version == MPEG_2) {
			h->bitrate = mpeg_bitrates[1][(head>>12)&0x0F];
			h->samplerate = mpeg_samplerates[1][(head>>10)&0x03];
		} else {
			h->bitrate = mpeg_bitrates[1][(head>>12)&0x0F];
			h->samplerate = mpeg_samplerates[2][(head>>10)&0x03];
		}
		h->padding = ((head>>9)&0x01) ? true:false;
		h->is_private = ((head>>8)&0x01) ? true:false;
		h->channels = (head>>6)&0x03;
		h->mode_ext = (head>>4)&0x03;
		h->copyright = ((head>>3)&0x01) ? true:false;
		h->original = ((head>>2)&0x01) ? true:false;
		h->emphasis = head&0x03;
		if ((head & 0xFFFF0000) != 0xFFFB0000) {
			head=head;
		}
		int totlen = h->bitrate * 144000;
		totlen /= h->samplerate << (h->version == MPEG_1 ? 0:1);
		if (h->padding) { totlen++; }
		//int totlen = (144 * (h->bitrate*1000) / h->samplerate) + (h->padding ? 1:0);// + (h->protection ? 2:0);
		if (totlen > MP3_MAX_FRAME_SIZE) {
			memmove(buf->data, buf->data + 1, buf->len - 1);
			buf->len--;
			buf->filepos++;
			continue;
		}
		if (totlen > buf->len) {
			int n2 = read_mp3_data(fp, buf);
			if (n2 == 0) {
				break;
			}
			if (n2 == -1) {
				ret = MP3_ERROR;
				break;
			}
		} else {
			memcpy(frame, buf->data, totlen);
			memmove(buf->data, buf->data + totlen, buf->len - totlen);
			*len = totlen;
			buf->len -= totlen;
			buf->filepos += totlen;
			ret = MP3_OK;
			break;
		}
	}
	return ret;
}

class MP3_Decoder: public SimpleDJ_Decoder {
private:
	buffer mbuf;
	uint8 frame[MP3_MAX_FRAME_SIZE];

	int channels;
	int samplerate;
	READER_HANDLE * fp;

	int samples_done;
	time_t start_time;
	int song_time;
public:
	virtual ~MP3_Decoder() {

	}

	virtual bool Open(READER_HANDLE * fnIn, TITLE_DATA * meta, int64 startpos) {
		memset(&mbuf, 0, sizeof(mbuf));
		samples_done = song_time = 0;

		fp = fnIn;

		channels = api->GetOutputChannels(0) > 1 ? 2:1;
		samplerate = api->GetOutputSample(0);
		/*
		if (rate != api->GetOutputSample(0)) {
			string x = _("SimpleDJ (mp3_decoder) -> WARNING: Samplerate does not match output samplerate in ircbot.conf!!!");
			api->botapi->LogToChan(x.c_str());
			api->botapi->ib_printf("%s\n", x.c_str());
		}
		if (a != channels) {
			string x = _("SimpleDJ (mp3_decoder) -> WARNING: Number of channels does not match output channels in ircbot.conf!!!");
			api->botapi->LogToChan(x.c_str());
			api->botapi->ib_printf("%s\n", x.c_str());
		}
		*/
		if (fp->fileSize) {
			int64 len = fp->fileSize;
			if (len >= 0) {
				len /= (api->GetOutputBitrate(0)/8);
				api->Timing_Reset((int32)len);
				char timetmp[64];
				api->PrettyTime((int32)(len/1000), timetmp);
				api->botapi->ib_printf(_("SimpleDJ (mp3_decoder) -> File length: %s\n"), timetmp);
			} else {
				api->botapi->ib_printf(_("SimpleDJ (mp3_decoder) -> Could not guess file length! (file size not available)\n"));
			}
		}

		mbuf.data = (uint8 *)zmalloc(MP3_BUFFER_SIZE);

		if (meta && strlen(meta->title)) {
			api->QueueTitleUpdate(meta);
		} else {
			api->botapi->ib_printf("SimpleDJ (mp3_decoder) -> ID3 Tag: No\n");
			TITLE_DATA *td = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
			memset(td,0,sizeof(TITLE_DATA));
			if (!stricmp(fnIn->type, "file")) {
				char * p = strrchr(fnIn->fn, '\\');
				if (!p) {
					p = strrchr(fnIn->fn, '/');
				}
				if (p) {
					sstrcpy(td->title, p+1);
				} else {
					sstrcpy(td->title, fnIn->fn);
				}
			} else {
				sstrcpy(td->title, fnIn->fn);
			}
			api->QueueTitleUpdate(td);
			zfree(td);
		}

		if (startpos) {
			fp->seek(fp, startpos, SEEK_SET);
		}

		start_time = time(NULL);
		return true;
	}

	virtual bool Open_URL(const char * url, TITLE_DATA * meta, int64 startpos) {
		return false;
	}

	virtual int64 GetPosition() {
		return mbuf.filepos;// fp ? fp->tell(fp):0;
	}

	virtual DECODE_RETURN Decode() {
		MP3_HEADER h;
		uint32 last_hdr=0;
		int len;
		//FILE * tfp = fopen("out.mp3", "wb");
		while (!api->ShouldIStop()) {
			int n = read_mp3_frame(fp, &mbuf, &h, frame, &len, &last_hdr);
			if (n == MP3_OK) {
				//api->botapi->ib_printf("%08X %08X %d %d\n", *cur_hdr, last_hdr, h.samplerate, h.channels);
				/*
				if (tfp) {
					fwrite(frame, len, 1, tfp);
				}
				*/
				if (!api->GetMixer()->raw(frame, len)) {
					/*
					if (tfp) {
						fclose(tfp);
					}
					*/
					return SD_DECODE_ERROR;
				}
				samples_done += 1152;
				while (samples_done >= h.samplerate) {
					song_time++;
					samples_done -= h.samplerate;
					api->Timing_Add(1000);
				}
			} else if (n == MP3_DONE) {
				break;
			} else {
				api->botapi->ib_printf("SimpleDJ -> Unknown return from read_mp3_frame(): %d\n", n);
				/*
				if (tfp) {
					fclose(tfp);
				}
				*/
				return SD_DECODE_ERROR;
				break;
			}

			while (song_time > (time(NULL)-start_time)) {
				if (api->ShouldIStop()) {
					return SD_DECODE_DONE;
				}
				safe_sleep(100, true);
			}
		}
		/*
		if (tfp) {
			fclose(tfp);
		}
		*/
		return SD_DECODE_DONE;
	}

	virtual void Close() {
		api->botapi->ib_printf("SimpleDJ (mp3_decoder) -> Close()\n");
		if (mbuf.data) {
			zfree(mbuf.data);
			mbuf.data = NULL;
		}
		api->Timing_Done();
	}
};

SimpleDJ_Decoder * mp3_create() { return new MP3_Decoder; }
void mp3_destroy(SimpleDJ_Decoder * dec) { delete dynamic_cast<MP3_Decoder *>(dec); }

DECODER mp3_decoder = {
	mp3_is_my_file,
	mp3_get_title_data,
	mp3_create,
	mp3_destroy
};

bool init(SD_PLUGIN_API * pApi) {
	api = pApi;
	api->RegisterDecoder(&mp3_decoder);
	return true;
};

void quit() {
};

SD_PLUGIN plugin = {
	SD_PLUGIN_VERSION,
	"MP3 Decoder",

	init,
	NULL,
	quit
};

PLUGIN_EXPORT SD_PLUGIN * GetSDPlugin() { return &plugin; }
