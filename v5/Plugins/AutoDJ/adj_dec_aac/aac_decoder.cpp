//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2023 Drift Solutions / Indy Sams            |
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
//#include <faad.h>
#include <neaacdec.h>

#if defined(WIN32)
#if defined(DEBUG)
#pragma comment(lib, "faad_d.lib")
#else
#pragma comment(lib, "faad.lib")
#endif
#endif

#define AAC_BUFFER_SIZE 524288
#define MAX_CHANNELS 6 /* make this higher to support files with
						  more channels */

struct buffer {
  unsigned char const *start;
  uint32 length;
};

AD_PLUGIN_API * adapi=NULL;

bool aac_is_my_file(const char * fn, const char * meta_type) {
	const char * ext = NULL;
	if (fn && (ext = strrchr(fn,'.')) != NULL ) {
		if (!stricmp(ext,".aac")) {
			return true;
		}
	}
	return false;
}

bool aac_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td,0,sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }

#ifdef WIN32
	char * p = (char *)strrchr(fn, '\\');
#else
	char * p = (char *)strrchr(fn, '/');
#endif
	if (p) {
		sstrcpy(td->title, p+1);
	} else {
		sstrcpy(td->title, fn);
	}
	if ((p = strrchr(td->title, '.'))) {
		p[0]=0;
	}

	strtrim(td->title," ");
	strtrim(td->artist," ");

	if (songlen) {
		/*
		SF_INFO info2;
		memset(&info2, 0, sizeof(info2));
		SNDFILE * sf2 = sf_open(fn, SFM_READ, &info2);
		if (sf2) {
			aac_count_t tmp = info2.frames / info2.samplerate;
			*songlen = uint32(tmp);
			aac_close(sf2);
		}
		*/
	}

	return true;
}

class AAC_Decoder: public AutoDJ_Decoder {
private:
	NeAACDecHandle hDec = NULL;
	READER_HANDLE * fp = NULL;
	FILE * ffp = NULL;
	unsigned long samplerate = 0;
	unsigned char channels = 0;
	DSL_BUFFER file_buf = { 0 };
public:
	virtual ~AAC_Decoder();

	virtual bool Open(READER_HANDLE * fpp, int64 startpos);
	virtual bool Open_URL(const char * url, int64 startpos);

	virtual int64 GetPosition();
	virtual DECODE_RETURN Decode();
	virtual void Close();
};

AAC_Decoder::~AAC_Decoder(){}

int64 AAC_Decoder::GetPosition() {
	return 0;// sf ? aac_seek(sf, 0, SEEK_CUR) : 0;
}

bool AAC_Decoder::Open(READER_HANDLE * fnIn, int64 startpos) {
	fp = fnIn;
	hDec = NeAACDecOpen();
	if (hDec == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error creating decoder handle!\n"));
		return false;
	}

	auto config = NeAACDecGetCurrentConfiguration(hDec);
	config->defSampleRate = adapi->GetOutputSample();
	config->outputFormat = FAAD_FMT_16BIT;
	NeAACDecSetConfiguration(hDec, config);

	buffer_init(&file_buf);
	buffer_resize(&file_buf, AAC_BUFFER_SIZE);
	int32 bRead = fnIn->read(file_buf.udata, AAC_BUFFER_SIZE, fnIn);
	if (bRead < FAAD_MIN_STREAMSIZE) {
		buffer_free(&file_buf);
		NeAACDecClose(hDec);
		hDec = NULL;
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> File is too small!\n"));
		return false;
	}
	if (bRead < AAC_BUFFER_SIZE) {
		buffer_remove_front(&file_buf, AAC_BUFFER_SIZE - bRead);
	}

	long bused = NeAACDecInit(hDec, file_buf.udata, file_buf.len, &samplerate, &channels);
	if (bused < 0) {
		buffer_free(&file_buf);
		NeAACDecClose(hDec);
		hDec = NULL;
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error opening file with decoder!\n"));
		return false;
	}
	buffer_remove_front(&file_buf, bused);

	if (startpos && fp->seek(fp, startpos, SEEK_SET) == startpos) {
		buffer_clear(&file_buf);
	}

	NeAACDecPostSeekReset(hDec, 1);
	return adapi->GetDeck(deck)->SetInAudioParams(channels, samplerate);
}

bool AAC_Decoder::Open_URL(const char * fn, int64 startpos) {
	if (access(fn, 0) != 0) {
		return false;
	}

	ffp = fopen(fn, "rb");
	if (ffp == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error opening '%s': %s!\n"), fn, strerror(errno));
		return false;
	}

	hDec = NeAACDecOpen();
	if (hDec == NULL) {
		fclose(ffp);
		ffp = NULL;
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error creating decoder handle!\n"));
		return false;
	}

	auto config = NeAACDecGetCurrentConfiguration(hDec);
	config->defSampleRate = adapi->GetOutputSample();
	config->outputFormat = FAAD_FMT_16BIT;
	NeAACDecSetConfiguration(hDec, config);

	buffer_init(&file_buf);
	buffer_resize(&file_buf, AAC_BUFFER_SIZE);
	size_t bRead = fread(file_buf.udata, 1, AAC_BUFFER_SIZE, ffp);
	if (bRead < FAAD_MIN_STREAMSIZE) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error reading file: %s!\n"), strerror(errno));
		if (bRead > 0) {
			adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> File is too small!\n"));
		} else {
			adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error reading file: %s!\n"), strerror(errno));
		}
		buffer_free(&file_buf);
		NeAACDecClose(hDec);
		hDec = NULL;
		fclose(ffp);
		ffp = NULL;
		return false;
	}
	if (bRead < AAC_BUFFER_SIZE) {
		buffer_remove_front(&file_buf, AAC_BUFFER_SIZE - bRead);
	}

	long bused = NeAACDecInit(hDec, file_buf.udata, file_buf.len, &samplerate, &channels);
	if (bused < 0) {
		buffer_free(&file_buf);
		NeAACDecClose(hDec);
		hDec = NULL;
		fclose(ffp);
		ffp = NULL;
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error opening file with decoder!\n"));
		return false;
	}
	buffer_remove_front(&file_buf, bused);

	if (startpos && fseek64(ffp, startpos, SEEK_SET) == startpos) {
		buffer_clear(&file_buf);
	}

	NeAACDecPostSeekReset(hDec, 1);
	return adapi->GetDeck(deck)->SetInAudioParams(channels, samplerate);
}

inline bool advance_to_frame(DSL_BUFFER * buf) {
	while (buf->len > 1) {
		uint8 * p = (uint8 *)memchr(buf->udata, 0xFF, buf->len);
		if (p == NULL) {
			return false;
		}
		size_t diff = p - buf->udata;
		if (diff) {
			buffer_remove_front(buf, diff);
			if (buf->len < 2) {
				return false;
			}
		}
		if ((buf->data[1] & 0xF6) == 0xF0) {
			return true;
		}
		// not a valid frame, remove the leading 0xFF and resume search
		buffer_remove_front(buf, 1);
	}
	return false;
}

DECODE_RETURN AAC_Decoder::Decode() {
	NeAACDecFrameInfo fi;
	if (file_buf.len < AAC_BUFFER_SIZE * 0.5) {
		if (fp != NULL && !fp->eof(fp)) {
			int32 toRead = AAC_BUFFER_SIZE - file_buf.len;
			char * buf = (char *)malloc(toRead);
			int32 bRead = fp->read(buf, toRead, fp);
			if (bRead > 0) {
				buffer_append(&file_buf, buf, bRead);
			}
			free(buf);
		} else if (ffp != NULL && !feof(ffp)) {
			int32 toRead = AAC_BUFFER_SIZE - file_buf.len;
			char * buf = (char *)malloc(toRead);
			int32 bRead = fread(buf, 1, toRead, ffp);
			if (bRead > 0) {
				buffer_append(&file_buf, buf, bRead);
			}
			free(buf);
		}
	}

	if (!advance_to_frame(&file_buf)) {
		//end of file
		return AD_DECODE_DONE;
	}

	auto sample_buffer = NeAACDecDecode(hDec, &fi, file_buf.udata, file_buf.len);
	buffer_remove_front(&file_buf, fi.bytesconsumed);

	if (fi.error) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error while decoding file: %s\n"), NeAACDecGetErrorMessage(fi.error));
		if (fi.bytesconsumed == 0) {
			buffer_remove_front(&file_buf, 1);
		}
		return AD_DECODE_CONTINUE;
	}

	if (sample_buffer == NULL) {
		//end of file
		return AD_DECODE_DONE;
	}

	if (fi.samplerate != samplerate || fi.channels != channels) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> AAC parameter change: %d channels, %dkHz samplerate!\n"), fi.channels, fi.samplerate);
		samplerate = fi.samplerate;
		channels = fi.channels;
		if (!adapi->GetDeck(deck)->SetInAudioParams(channels, samplerate)) {
			adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> SetInAudioParams(%d,%d) returned false!\n"), channels, samplerate);
			return AD_DECODE_ERROR;
		}
	}

	return (adapi->GetDeck(deck)->AddSamples((short *)sample_buffer, fi.samples / fi.channels) > 0) ? AD_DECODE_CONTINUE : AD_DECODE_DONE;
}

void AAC_Decoder::Close() {
	adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> aac_close()\n"));
	if (hDec != NULL) {
		NeAACDecClose(hDec);
		hDec = NULL;
		buffer_free(&file_buf);
	}
	if (ffp != NULL) {
		fclose(ffp);
		ffp = NULL;
	}
}

AutoDJ_Decoder * aac_create() { return new AAC_Decoder; }
void aac_destroy(AutoDJ_Decoder * dec) { delete dynamic_cast<AAC_Decoder *>(dec); }

DECODER aac_decoder = {
	1,
	aac_is_my_file,
	aac_get_title_data,
	aac_create,
	aac_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterDecoder(&aac_decoder);
	char *faad_id_string;
	char *faad_copyright_string;
	NeAACDecGetVersion(&faad_id_string, &faad_copyright_string);
	adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Using FAAD %s\n"), faad_id_string);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"AAC Decoder",

	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
