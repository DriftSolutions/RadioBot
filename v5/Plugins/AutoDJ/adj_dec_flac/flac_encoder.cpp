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

class FLAC_Encoder : public AutoDJ_Encoder {
private:
	Titus_Mutex hMutex;
	FLAC__StreamEncoder * encoder;
	FLAC__StreamMetadata * metadata;
	bool use_ogg;
	bool sendBufferedOutput();
	TITLE_DATA td;

public:
	FLAC_Encoder(bool use_ogg);
	virtual ~FLAC_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
	virtual void OnTitleUpdate(const char * title, TITLE_DATA * td);

	DSL_BUFFER buffer;
};

FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data) {
	FLAC_Encoder * enc = (FLAC_Encoder *)client_data;
	buffer_append(&enc->buffer, (const char *)buffer, bytes);
	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC_Encoder::FLAC_Encoder(bool do_use_ogg) {
	AutoMutex(hMutex);
	encoder = NULL;
	use_ogg = do_use_ogg;

	memset(&td, 0, sizeof(td));
	buffer_init(&buffer, false);
}

FLAC_Encoder::~FLAC_Encoder() {
	AutoMutex(hMutex);
	buffer_free(&buffer);
}

bool FLAC_Encoder::Init(int channels, int samplerate, float scale) {
	AutoMutex(hMutex);

	buffer_clear(&buffer);

	encoder = FLAC__stream_encoder_new();
	if (encoder == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (flac_encoder) -> Error allocating FLAC encoder!\n"));
		return false;
	}

	FLAC__stream_encoder_set_bits_per_sample(encoder, 16);
	FLAC__stream_encoder_set_sample_rate(encoder, samplerate);
	FLAC__stream_encoder_set_channels(encoder, channels);

	char buf[256];
	metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	snprintf(buf, sizeof(buf), "ShoutIRC.com AutoDJ - RadioBot v%s", adapi->botapi->GetVersionString());
	FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "COMMENT", buf);
	FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "DESCRIPTION", buf);
	FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	if (td.title[0]) {
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "TITLE", td.title);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	}
	if (td.album[0]) {
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ALBUM", td.album);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	}
	if (td.artist[0]) {
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", td.artist);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	}
	if (td.album_artist[0]) {
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ALBUM ARTIST", td.album_artist);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	}
	if (td.genre[0]) {
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "GENRE", td.genre);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	}
	if (td.track_no > 0) {
		snprintf(buf, sizeof(buf), "%u", td.track_no);
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "TRACKNUMBER", buf);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	}
	if (td.year > 0) {
		snprintf(buf, sizeof(buf), "%u", td.year);
		FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", buf);
		FLAC__metadata_object_vorbiscomment_append_comment(metadata, entry, /*copy=*/false);
	}
	FLAC__stream_encoder_set_metadata(encoder, &metadata, 1);

	FLAC__StreamEncoderInitStatus iret;
	if (use_ogg) {
		long x = adapi->botapi->genrand_int32() & 0x7FFFFFFF;
		FLAC__stream_encoder_set_ogg_serial_number(encoder, x);
		iret = FLAC__stream_encoder_init_ogg_stream(encoder, NULL, write_callback, NULL, NULL, NULL, this);
	} else {
		iret = FLAC__stream_encoder_init_stream(encoder, write_callback, NULL, NULL, NULL, this);
	}

	if (iret != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		adapi->botapi->ib_printf(_("AutoDJ (flac_encoder) -> Error initializing FLAC stream: %s\n"), FLAC__StreamEncoderInitStatusString[iret]);
		FLAC__stream_encoder_delete(encoder);
		encoder = NULL;
		FLAC__metadata_object_delete(metadata);
		metadata = NULL;
		return false;
	}

	adapi->botapi->ib_printf(_("AutoDJ (flac_encoder) -> Using libFLAC, stream initialized...\n"));
	//	adapi->botapi->ib_printf(_("AutoDJ (flac_encoder) -> Using LAME %d.%d\n"), lvp.major, lvp.minor);
	return true;
}

bool FLAC_Encoder::Raw(unsigned char * data, int32 len) {
	AutoMutex(hMutex);
	return adapi->GetFeeder()->Send(data, len);
}

bool FLAC_Encoder::sendBufferedOutput() {
	bool ret = true;
	if (buffer.len > 0) {
		ret = adapi->GetFeeder()->Send(buffer.udata, buffer.len);
		buffer_clear(&buffer);
	}
	return ret;
}

bool FLAC_Encoder::Encode(int samples, const short buffer[]) {
	AutoMutex(hMutex);

	if (encoder == NULL) {
		if (!Init(adapi->GetOutputChannels(), adapi->GetOutputSample(), 1.0)) {
			return false;
		}
	}

	int total = samples * adapi->GetOutputChannels();
	FLAC__int32 * pcm = new FLAC__int32[total];
	for (int i = 0; i < total; i++) {
		pcm[i] = buffer[i];
	}

	if (!FLAC__stream_encoder_process_interleaved(encoder, pcm, samples)) {
		adapi->botapi->ib_printf(_("Error occured while encoding FLAC Data: %s\n"), FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
		delete[] pcm;
		return false;
	}

	delete[] pcm;

	return sendBufferedOutput();
	/*
#ifdef WIN32
	__try {
#endif
		if (inChannels == 1) { // lame_encode_buffer_interleaved doesn't seem to be able to handle mono in any way, as an input or output
			len = lame_encode_buffer(gfp, (short *)buffer, (short *)buffer, samples, buf, bsize);
		} else {
			len = lame_encode_buffer_interleaved(gfp, (short *)buffer, samples, buf, bsize);
		}
#ifdef WIN32
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		adapi->botapi->ib_printf(_("ERROR: Exception occured while encoding FLAC Data!\n"));
		len = -1;
	}
#endif
	*/
}

void FLAC_Encoder::Close() {
	AutoMutex(hMutex);

	if (encoder) {
		FLAC__stream_encoder_finish(encoder);
		FLAC__stream_encoder_delete(encoder);
		encoder = NULL;
	}

	if (metadata) {
		FLAC__metadata_object_delete(metadata);
		metadata = NULL;
	}

	while (buffer.len > 0 && sendBufferedOutput()) {}
	buffer_clear(&buffer);
}

void FLAC_Encoder::OnTitleUpdate(const char * title, TITLE_DATA * tdd) {
	if (tdd && tdd->title[0]) {
		adapi->botapi->ib_printf(_("AutoDJ (flac_encoder) -> Closing stream to send new metadata...\n"));
		AutoMutex(hMutex);
		memcpy(&td, tdd, sizeof(td));
		Close();
	}
}

AutoDJ_Encoder * flac_ogg_enc_create() { return new FLAC_Encoder(true); }
AutoDJ_Encoder * flac_native_enc_create() { return new FLAC_Encoder(false); }
void flac_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<FLAC_Encoder *>(enc);
}

ENCODER ogg_flac_encoder = {
	"oggflac",
	"audio/ogg",
	50,

	flac_ogg_enc_create,
	flac_enc_destroy
};

ENCODER native_flac_encoder = {
	"flac",
	"audio/flac",
	50,

	flac_native_enc_create,
	flac_enc_destroy
};
