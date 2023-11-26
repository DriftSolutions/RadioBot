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

#include <taglib.h>
#include <tag.h>
#include <fileref.h>
#include <mpegfile.h>
#include <id3v2tag.h>
#if TAGLIB_MAJOR_VERSION > 1 || (TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION >= 8)
#define TAGLIB_HAVE_DEBUG
#include <tdebuglistener.h>
#endif
#if TAGLIB_MAJOR_VERSION > 1 || (TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION > 13) || (TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION == 13 && TAGLIB_PATCH_VERSION > 1)
#define TAGLIB_HAVE_AAC true
#else
#define TAGLIB_HAVE_AAC false
#endif

#if defined(WIN32)
#if defined(DEBUG)
#pragma comment(lib, "faad_d.lib")
#pragma comment(lib, "taglib_d.lib")
#else
#pragma comment(lib, "faad.lib")
#pragma comment(lib, "taglib.lib")
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

inline bool aac_finish_get_title_data(TagLib::MPEG::File * iTag, TITLE_DATA * td, uint32 * songlen) {
	bool ret = false;

	if (iTag->tag() && !iTag->tag()->isEmpty()) {
		TagLib::String p = iTag->tag()->title();
		if (!p.isNull()) {
			strlcpy(td->title, p.toCString(true), sizeof(td->title));
		}
		p = iTag->tag()->artist();
		if (!p.isNull()) {
			strlcpy(td->artist, p.toCString(true), sizeof(td->artist));
		}
		p = iTag->tag()->album();
		if (!p.isNull()) {
			strlcpy(td->album, p.toCString(true), sizeof(td->album));
		}
		p = iTag->tag()->genre();
		if (!p.isNull()) {
			strlcpy(td->genre, p.toCString(true), sizeof(td->genre));
		}
		p = iTag->tag()->comment();
		if (!p.isNull()) {
			strlcpy(td->comment, p.toCString(true), sizeof(td->comment));
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
		if (ap != NULL) {
			*songlen = ap->length();
		}
	}

	return ret;
}

bool aac_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen = 0; }
	if (adapi->getID3Mode() == 0) {
		return false;
	}
	bool ret = false;

#if defined(WIN32)
	char buf[MAX_PATH] = { 0 };
	if (access(fn, 0) != 0) {
		GetShortPathName(fn, buf, sizeof(buf));
		if (strlen(buf) && access(buf, 0) == 0) {
			fn = buf;
		}
	}
#endif
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		char buf2[4] = { 0,0,0,0 };
		size_t n = fread(buf2, 3, 1, fp);
		fclose(fp);
		if (n == 1 && !strcmp(buf2, "ID3")) {
			TagLib::MPEG::File iTag(fn, TAGLIB_HAVE_AAC);
			if (iTag.isOpen()) {
				ret = aac_finish_get_title_data(&iTag, td, songlen);
			}
		}
	}

	return ret;
}

class AAC_Decoder: public AutoDJ_Decoder {
private:
	NeAACDecHandle hDec = NULL;

	READER_HANDLE * fp = NULL;
	FILE * ffp = NULL;
	DSL_BUFFER file_buf = { 0 };

	unsigned long samplerate = 0;
	unsigned char channels = 0;
	uint8 error_count = 0;

	bool finishOpen();
	int32 read(void * buf, int32 size) {
		if (fp != NULL) {
			return fp->read(buf, size, fp);
		} else if (ffp != NULL) {
			return fread(buf, 1, size, ffp);
		}
		return -1;
	}
	bool eof() {
		if (fp != NULL) {
			return fp->eof(fp);
		} else if (ffp != NULL) {
			return feof(ffp);
		}
		return true;
	}
	void fill_buffer() {
		if (file_buf.len < AAC_BUFFER_SIZE && !eof()) {
			int32 toRead = AAC_BUFFER_SIZE - file_buf.len;
			char * buf = (char *)malloc(toRead);
			int32 bRead = this->read(buf, toRead);
			if (bRead > 0) {
				buffer_append(&file_buf, buf, bRead);
			}
			free(buf);
		}
	}
public:
	virtual ~AAC_Decoder() { Close(); }

	virtual bool Open(READER_HANDLE * fpp, int64 startpos);
	virtual bool Open_URL(const char * url, int64 startpos);

	virtual int64 GetPosition();
	virtual DECODE_RETURN Decode();
	virtual void Close();
};

int64 AAC_Decoder::GetPosition() {
	return 0;// sf ? aac_seek(sf, 0, SEEK_CUR) : 0;
}

bool AAC_Decoder::Open(READER_HANDLE * fnIn, int64 startpos) {
	fp = fnIn;
	return finishOpen();
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

	return finishOpen();
}

bool AAC_Decoder::finishOpen() {
	hDec = NeAACDecOpen();
	if (hDec == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error creating decoder handle!\n"));
		return false;
	}

	buffer_init(&file_buf);
	buffer_resize(&file_buf, AAC_BUFFER_SIZE);
	int32 bRead = this->read(file_buf.udata, AAC_BUFFER_SIZE);
	if (bRead < FAAD_MIN_STREAMSIZE) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> File is too small!\n"));
		return false;
	}
	if (bRead < AAC_BUFFER_SIZE) {
		buffer_resize(&file_buf, bRead);
	}

	uint32 tagsize = 0;
	if (!memcmp(file_buf.udata, "ID3", 3)) {
		// skip over the ID3 tag if we have one
		tagsize = (file_buf.udata[6] << 21) | (file_buf.udata[7] << 14) | (file_buf.udata[8] << 7) | (file_buf.udata[9] << 0);
		tagsize += 10;
#ifdef DEBUG
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Found ID3 tag with size %u...\n"), tagsize);
#endif
		buffer_remove_front(&file_buf, tagsize);
		fill_buffer();
	}

	auto config = NeAACDecGetCurrentConfiguration(hDec);
	config->defSampleRate = adapi->GetOutputSample();
	config->outputFormat = FAAD_FMT_16BIT;
	NeAACDecSetConfiguration(hDec, config);

	long bused = NeAACDecInit(hDec, file_buf.udata, file_buf.len, &samplerate, &channels);
	if (bused < 0) {
		adapi->botapi->ib_printf(_("AutoDJ (aac_decoder) -> Error opening file with decoder!\n"));
		return false;
	}
	buffer_remove_front(&file_buf, bused);

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
		fill_buffer();
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
		return (error_count++ < 5) ? AD_DECODE_CONTINUE : AD_DECODE_ERROR;
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
