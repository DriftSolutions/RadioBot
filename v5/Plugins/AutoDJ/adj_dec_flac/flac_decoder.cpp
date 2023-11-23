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

#include "flac_plugin.h"

AD_PLUGIN_API * adapi=NULL;

class FLAC_Decoder;
struct FLAC_HANDLE {
	READER_HANDLE * fp;

	//used when decoding
	FLAC_Decoder * dec;

	//used when getting meta data
	TITLE_DATA * td;
	uint32 songlen;
};


FLAC__StreamDecoderReadStatus flac_read(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
FLAC__StreamDecoderSeekStatus flac_seek(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
FLAC__StreamDecoderTellStatus flac_tell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
FLAC__StreamDecoderLengthStatus flac_length(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
FLAC__bool flac_eof(const FLAC__StreamDecoder *decoder, void *client_data);
void flac_meta(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *md, void *client_data);
void flac_meta_get_title_data(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *md, void *client_data);
void flac_error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
FLAC__StreamDecoderWriteStatus flac_write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);

bool flac_is_my_file(const char * fn, const char * mime_type) {
	if (mime_type) {
		if (!stricmp(mime_type, "application/ogg") || !stricmp(mime_type, "audio/vorbis") || !stricmp(mime_type, "audio/ogg") || !stricmp(mime_type, "application/flac") || !stricmp(mime_type, "audio/flac") || !stricmp(mime_type, "audio/flac")) {
			return true;
		}
	}
	if (fn && strrchr(fn,'.') && (!stricmp(strrchr(fn,'.'),".ogg") || !stricmp(strrchr(fn,'.'),".flac"))) {//
		return true;
	}
	return false;
}

void flac_meta_get_title_data(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *md, void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (md->type == FLAC__METADATA_TYPE_STREAMINFO) {
		if (md->data.stream_info.total_samples > 0) {
			FLAC__uint64 len = md->data.stream_info.total_samples;
			len /= md->data.stream_info.sample_rate;
			h->songlen = len;
		}
	} else if (md->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
		for (FLAC__uint32 i=0; i < md->data.vorbis_comment.num_comments; i++) {
			const char * p = (const char *)md->data.vorbis_comment.comments[i].entry;
			if (!strnicmp(p,"title=",6)) {
				strlcpy(h->td->title,p+6,sizeof(h->td->title));
			}
			if (!strnicmp(p,"artist=",7)) {
				strlcpy(h->td->artist,p+7,sizeof(h->td->artist));
			}
			if (!strnicmp(p,"album=",6)) {
				strlcpy(h->td->album,p+6,sizeof(h->td->album));
			}
			if (!strnicmp(p,"genre=",6) && !strlen(h->td->genre)) {
				strlcpy(h->td->genre,p+6,sizeof(h->td->genre));
			}
			if (!strnicmp(p,"url=",4) && !strlen(h->td->url)) {
				strlcpy(h->td->url,p+4,sizeof(h->td->url));
			}
			if (!strnicmp(p,"DESCRIPTION=",12) && strempty(h->td->comment)) {
				strlcpy(h->td->comment,p+12,sizeof(h->td->comment));
			}
			if (!strnicmp(p,"TRACKNUMBER=",12) && h->td->track_no == 0) {
				h->td->track_no = atoi(p+12);
			}
			if (!strnicmp(p,"YEAR=",5) && h->td->year == 0) {
				h->td->year = atoi(p+5);
			}

			//printf("%s\n",md->data.vorbis_comment.comments[i].entry);
		}
		//client_data=client_data;
	//} else {
		//client_data=client_data;
	}
}

bool flac_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }

	FLAC_HANDLE fh;
	memset(&fh, 0, sizeof(fh));
	fh.td = td;

	FLAC__StreamDecoder * decoder = FLAC__stream_decoder_new();
	if (decoder == NULL) {
		adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error allocating new decoder for %s!\n", fn);
		return false;
	}
	FLAC__stream_decoder_set_md5_checking(decoder, false);
	FLAC__stream_decoder_set_metadata_respond_all(decoder);

	if (FLAC__stream_decoder_init_file(decoder, fn, flac_write, flac_meta_get_title_data, flac_error, &fh) != FLAC__STREAM_DECODER_INIT_STATUS_OK && FLAC__stream_decoder_init_ogg_file(decoder, fn, flac_write, flac_meta_get_title_data, flac_error, &fh) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error initializing decoder for %s!\n", fn);
		FLAC__stream_decoder_delete(decoder);
		return false;
	}

	if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
		adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error reading metadata in %s!\n", fn);
		FLAC__stream_decoder_delete(decoder);
		return false;
	}

	if (fh.songlen && songlen) {
		*songlen = fh.songlen;
	}

	FLAC__stream_decoder_delete(decoder);
	return true;
}

class FLAC_Decoder: public AutoDJ_Decoder {
private:
	READER_HANDLE * fp;
	FLAC_HANDLE fh;
	FLAC__StreamDecoder *decoder;
	int32 song_length;
public:
	unsigned int channels;
	FLAC__uint64 total_samples;
	unsigned int sample_rate;
	int bps;

	virtual ~FLAC_Decoder() {

	}

	bool Open_Finish(int64 startpos) {
		if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
			adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error reading metadata!\n");
			FLAC__stream_decoder_delete(decoder);
			decoder = NULL;
			return false;
		}

		if (channels < 1) {
			adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Sorry, we only support mono or higher audio files!\n");
			FLAC__stream_decoder_delete(decoder);
			decoder = NULL;
			return false;
		}

		if (bps < 8 || bps > 31) {
			adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Sorry, we only support sample sizes from 8-31 bits (this song uses %d)!\n", bps);
			FLAC__stream_decoder_delete(decoder);
			decoder = NULL;
			return false;
		}

		if (total_samples != 0) {
			FLAC__uint64 len = total_samples/sample_rate;
			song_length = len*1000;
			SetLength(song_length);
		}

		adapi->botapi->ib_printf(_("AutoDJ (flac_decoder) -> Bitstream is %d channel, %dHz\n"),channels,sample_rate);
		adapi->botapi->ib_printf(_("AutoDJ (flac_decoder) -> Decoded length: " U64FMT " samples\n"),total_samples);
		adapi->botapi->ib_printf(_("AutoDJ (flac_decoder) -> Total playing time: %d seconds\n"),song_length/1000);

		if (startpos) {
			if (!FLAC__stream_decoder_seek_absolute(decoder, startpos) && FLAC__stream_decoder_get_state(decoder) == FLAC__STREAM_DECODER_SEEK_ERROR) {
				FLAC__stream_decoder_reset(decoder);
			}
		}

		return adapi->GetDeck(deck)->SetInAudioParams(channels,sample_rate);
	}

	virtual bool Open(READER_HANDLE * fnIn, int64 startpos) {
		memset(&fh, 0, sizeof(fh));
		fh.fp = fnIn;
		fh.dec = this;
		channels = 0;
		total_samples = 0;
		sample_rate = 0;
		bps = 0;
		song_length = 0;
		fp = fnIn;

		if ((decoder = FLAC__stream_decoder_new()) == NULL) {
			adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error allocating new decoder!\n");
			return false;
		}
		FLAC__stream_decoder_set_md5_checking(decoder, false);
		FLAC__stream_decoder_set_metadata_respond_all(decoder);

		if (FLAC__stream_decoder_init_stream(decoder, flac_read, flac_seek, flac_tell, flac_length, flac_eof, flac_write, flac_meta, flac_error, &fh) != FLAC__STREAM_DECODER_INIT_STATUS_OK && FLAC__stream_decoder_init_ogg_stream(decoder, flac_read, flac_seek, flac_tell, flac_length, flac_eof, flac_write, flac_meta, flac_error, &fh) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
			adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error initializing decoder!\n");
			FLAC__stream_decoder_delete(decoder);
			decoder = NULL;
			return false;
		}

		return Open_Finish(startpos);
	}

	virtual bool Open_URL(const char * url, int64 startpos) {
		memset(&fh, 0, sizeof(fh));
		fh.dec = this;
		channels = 0;
		total_samples = 0;
		sample_rate = 0;
		bps = 0;
		song_length = 0;
		fp = NULL;

		if ((decoder = FLAC__stream_decoder_new()) == NULL) {
			adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error allocating new decoder!\n");
			return false;
		}
		FLAC__stream_decoder_set_md5_checking(decoder, false);
		FLAC__stream_decoder_set_metadata_respond_all(decoder);

		if (FLAC__stream_decoder_init_file(decoder, url, flac_write, flac_meta, flac_error, &fh) != FLAC__STREAM_DECODER_INIT_STATUS_OK && FLAC__stream_decoder_init_ogg_file(decoder, url, flac_write, flac_meta, flac_error, &fh) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
			adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error initializing decoder!\n");
			FLAC__stream_decoder_delete(decoder);
			decoder = NULL;
			return false;
		}

		return Open_Finish(startpos);
	}

	virtual int64 GetPosition() {
		return fp ? fp->tell(fp):0;
	}

	virtual DECODE_RETURN Decode() {
		FLAC__stream_decoder_process_single(decoder);
		return AD_DECODE_DONE;
	}

	virtual void Close() {
		adapi->botapi->ib_printf(_("AutoDJ (flac_decoder) -> Close()\n"));
		if (decoder) {
			FLAC__stream_decoder_finish(decoder);
			FLAC__stream_decoder_delete(decoder);
			decoder = NULL;
		}
	}
};

FLAC__StreamDecoderReadStatus flac_read(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (h == NULL) {
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}
	int32 tmp = h->fp->read(buffer, *bytes, h->fp);
	*bytes = tmp;
	if (tmp <= 0) { return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM; }
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderSeekStatus flac_seek(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (h == NULL) {
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	}
	if (!h->fp->can_seek) {
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	}
	return (h->fp->seek(h->fp, absolute_byte_offset, SEEK_SET) == absolute_byte_offset) ? FLAC__STREAM_DECODER_SEEK_STATUS_OK:FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

FLAC__StreamDecoderTellStatus flac_tell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (h == NULL) {
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
	}
	int32 pos = h->fp->tell(h->fp);
	if (pos == -1L) {
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
	}
	*absolute_byte_offset = pos;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus flac_length(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (h == NULL) {
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	}
	if (!h->fp->can_seek || h->fp->fileSize == 0) {
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	}
	*stream_length = h->fp->fileSize;
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool flac_eof(const FLAC__StreamDecoder *decoder, void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (h == NULL) {
		return true;
	}
	return h->fp->eof(h->fp);
}

void flac_meta(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *md, void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (md->type == FLAC__METADATA_TYPE_STREAMINFO) {
		h->dec->bps = md->data.stream_info.bits_per_sample;
		h->dec->channels = md->data.stream_info.channels;
		h->dec->sample_rate = md->data.stream_info.sample_rate;
		h->dec->total_samples = md->data.stream_info.total_samples;
	}
}

void flac_error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
	adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> Error decoding FLAC stream: %s!\n", FLAC__StreamDecoderErrorStatusString[status]);
}

FLAC__StreamDecoderWriteStatus flac_write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data) {
	FLAC_HANDLE * h = (FLAC_HANDLE *)client_data;
	if (frame->header.channels != h->dec->channels || frame->header.sample_rate != h->dec->sample_rate) {
		adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> WARNING: Stream channels/samplerate changed mid-stream!\n");
		if (frame->header.channels < 1 || !adapi->GetDeck(h->dec->GetDeck())->SetInAudioParams(frame->header.channels,frame->header.sample_rate)) {
			h->dec->channels = frame->header.channels;
			h->dec->sample_rate = frame->header.sample_rate;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
	}

	short * buf = new short[(frame->header.blocksize*frame->header.channels)];
	short * ptr = buf;
	FLAC__int32 mask = (1 << frame->header.bits_per_sample) - 1;
	double dmask = mask;
	for (unsigned i = 0; i < frame->header.blocksize; i++) {
		for (unsigned j=0; j < frame->header.channels; j++) {
			double tmp = double(buffer[j][i] & mask) / dmask;
			*ptr = tmp * 65535;
			ptr++;
		}
	}
	adapi->GetDeck(h->dec->GetDeck())->AddSamples(buf, frame->header.blocksize);
	delete [] buf;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

AutoDJ_Decoder * flac_create() { return new FLAC_Decoder; }
void flac_destroy(AutoDJ_Decoder * dec) { delete dynamic_cast<FLAC_Decoder *>(dec); }

DECODER flac_decoder = {
	998,
	flac_is_my_file,
	flac_get_title_data,
	flac_create,
	flac_destroy
};

extern ENCODER ogg_flac_encoder;
extern ENCODER native_flac_encoder;

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterDecoder(&flac_decoder);
	adapi->RegisterEncoder(&native_flac_encoder);
	if (FLAC_API_SUPPORTS_OGG_FLAC == 1) {
		adapi->RegisterEncoder(&ogg_flac_encoder);
	} else {
		adapi->botapi->ib_printf("AutoDJ (flac_decoder) -> This version of libFLAC does not support Ogg FLAC streams, disabling 'oggflac' encoder...\n");
	}
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"FLAC Decoder",

	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
