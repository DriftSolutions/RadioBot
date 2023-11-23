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

AD_PLUGIN_API * adapi=NULL;
DL_HANDLE hDL = NULL;
Titus_Mutex tagMutex;

/*
struct buffer {
  unsigned char const *start;
  unsigned long length;
  AutoDJ_Decoder * dec;
};

static inline unsigned long prng(unsigned long state) {
	return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

struct audio_stats {
  unsigned long clipped_samples;
  mad_fixed_t peak_clipping;
  mad_fixed_t peak_sample;
};
struct audio_dither {
  mad_fixed_t error[3];
  mad_fixed_t random;
};
*/

bool mp3_is_my_file(const char * fn, const char * mime_type) {
	if (fn) {
		const char * ext = strrchr(fn,'.');
		if (ext && (!stricmp(ext,".mp3") || !stricmp(ext,".cm3")))   {
			return true;
		}
	}
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

#if defined(USE_TAGLIB)
bool mp3_finish_get_title_data(TagLib::MPEG::File * iTag, TITLE_DATA * td, uint32 * songlen) {
	bool ret = false;
	if (iTag->isOpen()) {
		if (iTag->tag() && !iTag->tag()->isEmpty()) {
			TagLib::String p = iTag->tag()->title();
			if (!p.isNull()) {
				strlcpy(td->title,p.toCString(true),sizeof(td->title));
			}
			p = iTag->tag()->artist();
			if (!p.isNull()) {
				strlcpy(td->artist,p.toCString(true),sizeof(td->artist));
			}
			p = iTag->tag()->album();
			if (!p.isNull()) {
				strlcpy(td->album,p.toCString(true),sizeof(td->album));
			}
			p = iTag->tag()->genre();
			if (!p.isNull()) {
				strlcpy(td->genre,p.toCString(true),sizeof(td->genre));
			}
			p = iTag->tag()->comment();
			if (!p.isNull()) {
				strlcpy(td->comment,p.toCString(true),sizeof(td->comment));
			}
			td->track_no = iTag->tag()->track();
			td->year = iTag->tag()->year();
			strtrim(td->title);
			strtrim(td->artist);
			strtrim(td->album);
			strtrim(td->genre);
			strtrim(td->comment);

			TagLib::ID3v2::Tag * tag2 = iTag->ID3v2Tag(false);
			if (tag2 != NULL) {
				TagLib::ID3v2::FrameList l = tag2->frameListMap()["TPE2"];
				if (!l.isEmpty()) {
					strlcpy(td->album_artist, l.front()->toString().toCString(true), sizeof(td->album_artist));
					strtrim(td->album_artist);
				}
			}
			if (strlen(td->title)) {
				ret = true;
			}
		}
		if (songlen) {
			TagLib::AudioProperties * ap = iTag->audioProperties();
			if (ap) {
				*songlen = ap->length();
			}
		}
	}
	return ret;
}
#endif

bool mp3_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	//tagMutex.Lock();
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }
	if (adapi->getID3Mode() == 0) {
		return false;
	}
	//uint32 ticks = GetTickCount();
	bool ret = false;

#if defined(USE_TAGLIB)
#if defined(WIN32)
	char buf[MAX_PATH]={0};
	if (access(fn, 0) != 0) {
		GetShortPathName(fn, buf, sizeof(buf));
		if (strlen(buf) && access(buf, 0) == 0) {
			fn = buf;
		}
	}
#endif
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		char * buf2 = (char *)malloc(4096);
		size_t n = fread(buf2, 1, 4096, fp);
		fclose(fp);
		bool all_zero = true;
		if (n > 0) {
			char *p = buf2;
			for (size_t i=0; i < n; i++) {
				if (*p++ != 0) {
					all_zero = false;
					break;
				}
			}
		}
		free(buf2);
		if (all_zero) {
			adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> '%s' begins with all zeros! Skipping ID3 read since it can cause a deadlock in taglib...\n", fn);
			return false;
		}
	}

	TagLib::MPEG::File iTag(fn);
	//TagLib::FileRef iTag(fn);
	if (iTag.isOpen()) {
		//ret = mp3_finish_get_title_data(iTag.file(), td, songlen);
		ret = mp3_finish_get_title_data(&iTag, td, songlen);
	}
#endif

	return ret;
}

ssize_t adj_mp3_read(void * fd, void * buf, size_t len) {
	READER_HANDLE * h = (READER_HANDLE *)fd;
	return h->read(buf, len, h);
}

off_t adj_mp3_seek(void * fd, off_t off, int mode) {
	READER_HANDLE * h = (READER_HANDLE *)fd;
	return h->seek(h, off, mode);
}

class MP3_Decoder: public AutoDJ_Decoder {
private:
	mpg123_handle *mh;
	SSMT_CTX * ctx;
	short * buffer;
	int read_size;
	bool first;
	Audio_Buffer * abuffer;
	SSMT_TAG * ctx_tag;
	int channels;

	virtual void parseSSMT(const char * fn) {
		ctx = LibSSMT_OpenFile(fn);
		if (ctx) {
			LibSSMT_ScanFile(ctx);
			adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> Number of SSMT Tags: %d\n"),ctx->num_tags);
			SSMT_TAG * Scan = ctx_tag = ctx->first_tag;
			while (Scan) {
				adapi->botapi->ib_printf(_("SSMT @ " I64FMT ": %d items\n"),Scan->filepos,Scan->num_items);
				Scan = Scan->Next;
			}
		} else {
			adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> Error opening file for LibSSMT...\n"));
		}
	}

	virtual bool setupOpen() {
		ctx = NULL;
		ctx_tag = NULL;
		mh = NULL;
		read_size = 0;
		buffer = NULL;
		song_length = 0;

		int err = 0;
		mh = mpg123_new(NULL, &err);
		if (mh == NULL) {
			adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> Error creating audio handle: %s!\n", mpg123_plain_strerror(err));
			return false;
		}

#if MPG123_API_VERSION >= 26
		//mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_SKIP_ID3V2, 0.);
#endif
		mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_QUIET, 0.);
		mpg123_param(mh, MPG123_RVA, MPG123_RVA_ALBUM, 0.);
		return true;
	}

	virtual bool finishOpen(int64 startpos) {
		mpg123_format_none(mh);
		channels = adapi->GetOutputChannels() > 1 ? MPG123_STEREO:MPG123_MONO;
		const long * list;
		size_t num;
		mpg123_rates(&list, &num);
		for (size_t i=0; i < num; i++) {
			mpg123_format(mh, list[i], channels, MPG123_ENC_SIGNED_16);
		}
		if (startpos) {
			mpg123_seek(mh, (off_t)startpos, SEEK_SET);
		}

		long rate;
		int a,encoding;
		if (mpg123_getformat(mh, &rate, &a, &encoding) != MPG123_OK) {
			adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> Could not determine sample format (is this really an MP3 file?)!\n");
			mpg123_close(mh);
			mpg123_delete(mh);
			mh = NULL;
			if (ctx) { LibSSMT_CloseFile(ctx); ctx=NULL; }
			return false;
		}
		if (encoding != MPG123_ENC_SIGNED_16) {
			adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> AutoDJ only supports 16-bit signed samples!\n");
			mpg123_close(mh);
			mpg123_delete(mh);
			mh = NULL;
			if (ctx) { LibSSMT_CloseFile(ctx); ctx=NULL; }
			return false;
		}
		adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> MP3 parameters: %d channels, %dHz samplerate.\n"), a, rate);
		if (GetLength() <= 0) {
			off_t len = mpg123_length(mh);
			if (len >= 0) {
				len /= rate;
				SetLength(len*1000);
				char timebuf[32];
				adapi->PrettyTime(song_length/1000, timebuf);
				adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> Guessed file length: %s\n"), timebuf);
			} else {
				adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> Could not guess file length! (%s)\n"), mpg123_strerror(mh));
			}
		}
		if (!adapi->GetDeck(deck)->SetInAudioParams(channels, rate)) {
			adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> Encoder init(%d,%d) returned false!\n"), channels, rate);
			mpg123_close(mh);
			mpg123_delete(mh);
			mh = NULL;
			if (ctx) { LibSSMT_CloseFile(ctx); ctx=NULL; }
			return false;
		}

		abuffer = adapi->AllocAudioBuffer();
		buffer = (short *)zmalloc(MP3_BUFFER_SIZE);
		read_size = mpg123_outblock(mh);
		return true;
	}

public:
	virtual ~MP3_Decoder() {

	}

	virtual bool Open(READER_HANDLE * fnIn, int64 startpos) {
		fp = fnIn;
#if MPG123_API_VERSION >= 24
		if (!setupOpen()) { return false; }

		if (!strcmp(fnIn->type,"file")) {
			parseSSMT(fnIn->fn);
		}

		/*
		int err = 0;
		mh = mpg123_new(NULL, &err);
		if (mh == NULL) {
			adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> Error creating audio handle: %s!\n", mpg123_plain_strerror(err));
			return false;
		}
		*/

		if (!fnIn->can_seek) {
			mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_SEEKBUFFER, 0.);
		}
		mpg123_replace_reader_handle(mh, adj_mp3_read, adj_mp3_seek, NULL);
		if (mpg123_open_handle(mh, fnIn) != MPG123_OK) {
			adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> Error creating audio handle: %s!\n", mpg123_strerror(mh));
			mpg123_delete(mh);
			mh = NULL;
			if (ctx) { LibSSMT_CloseFile(ctx); ctx=NULL; }
			return false;
		}

		return finishOpen(startpos);
#else
		return false;
#endif
	}

	virtual bool Open_URL(const char * url, int64 startpos) {
		if (!strnicmp("http://", url, 7) || !strnicmp("https://", url, 8)) {
			return false;
		}

		fp = NULL;
		if (!setupOpen()) { return false; }

		parseSSMT(url);

		if (mpg123_open(mh, (char *)url) != MPG123_OK) {
			adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> Error creating audio handle: %s!\n", mpg123_strerror(mh));
			mpg123_delete(mh);
			mh = NULL;
			if (ctx) { LibSSMT_CloseFile(ctx); ctx=NULL; }
			return false;
		}

		return finishOpen(startpos);
	}

	virtual int64 GetPosition() {
		return mh ? mpg123_tell(mh):0;
	}

	virtual int32 Decode() {
		if (ctx_tag && ((unsigned)fp->tell(fp) >= ctx_tag->filepos)) {
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
				adapi->QueueTitleUpdate(&td);
			} else {
				if ((it = LibSSMT_GetItem(ctx_tag, SSMT_SURL)) != NULL) {
					strlcpy(td.url, it->str, sizeof(td.url));
				}
				if ((it = LibSSMT_GetItem(ctx_tag, SSMT_STIT)) != NULL) {
					strlcpy(td.title, it->str, sizeof(td.title));
					adapi->QueueTitleUpdate(&td);
				}
			}
			ctx_tag = ctx_tag->Next;
		}

		size_t done;
		int n = mpg123_read(mh, (unsigned char *)buffer, MP3_BUFFER_SIZE, &done);
		if (n == MPG123_NEW_FORMAT) {
			long rate;
			int b;
			mpg123_getformat(mh, &rate, &channels, &b);
			adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> MP3 parameter change: %d channels, %dkHz samplerate!\n"), channels, rate);
			if (!adapi->GetDeck(deck)->SetInAudioParams(channels, rate)) {
				adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> SetInAudioParams(%d,%d) returned false!\n"), channels, rate);
				return AD_DECODE_ERROR;
			}
			return AD_DECODE_CONTINUE;
		} else if (n == MPG123_OK) {
			int32 samples = (channels == MPG123_MONO) ? done/2:done/4;
			//int32 rsamples = done/2;
			adapi->GetDeck(deck)->AddSamples(buffer, samples);
			return AD_DECODE_CONTINUE;
		} else if (n == MPG123_DONE) {
			return AD_DECODE_DONE;
		} else if (n == MPG123_NEED_MORE) {
			return AD_DECODE_DONE;
			//return AD_DECODE_CONTINUE;
		} else {
			adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> Decoder returned: %d (err: %s)\n"), n, mpg123_strerror(mh));
			return AD_DECODE_ERROR;
		}
	}

	virtual void Close() {
		adapi->botapi->ib_printf(_("AutoDJ (mp3_decoder) -> Close()\n"));
		if (mh) {
			mpg123_close(mh);
			mpg123_delete(mh);
			mh = NULL;
		}
		if (ctx) { LibSSMT_CloseFile(ctx); ctx=NULL; }
		if (buffer) {
			zfree(buffer);
			buffer = NULL;
		}
		if (abuffer) {
			adapi->FreeAudioBuffer(abuffer);
			abuffer = NULL;
		}
	}
};

//	mad.mad_timer_add(&mp3_current,header->duration);

AutoDJ_Decoder * mp3_create() { return new MP3_Decoder; }
void mp3_destroy(AutoDJ_Decoder * dec) { delete dynamic_cast<MP3_Decoder *>(dec); }

DECODER mp3_decoder = {
	0,
	mp3_is_my_file,
	mp3_get_title_data,
	mp3_create,
	mp3_destroy
};

#ifdef TAGLIB_HAVE_DEBUG
namespace TagLib {
	class ADJ_DebugListener : public DebugListener
	{
	public:
		ADJ_DebugListener() {}
		virtual ~ADJ_DebugListener() {}

		/*!
		* When overridden in a derived class, redirects \a msg to your preferred
		* channel such as stderr, Windows debugger or so forth.
		*/
		virtual void printMessage(const String &msg) {
#if defined(DEBUG)
			adapi->botapi->ib_printf("AutoDJ (taglib) -> %s", msg.to8Bit(true).c_str());
#endif
		}

		/*
	private:
		// Noncopyable
		DebugListener(const DebugListener &);
		DebugListener &operator=(const DebugListener &);
		*/
	};
};

TagLib::ADJ_DebugListener debug_listener;
#endif

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;

	int err = mpg123_init();
	if ( err != MPG123_OK) {
		adapi->botapi->ib_printf("AutoDJ (mp3_decoder) -> Error initializing mp3 decoder: %s\n", mpg123_plain_strerror(err));
		mpg123_exit();
		return false;
	}

#ifdef TAGLIB_HAVE_DEBUG
	TagLib::setDebugListener(&debug_listener);
#endif
	adapi->RegisterDecoder(&mp3_decoder);
	return true;
};

void quit() {
	mpg123_exit();
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"MP3 Decoder",

	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
