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

#include "opus_encoder.h"
#include <math.h>

#if defined(WIN32)
#if defined(DEBUG)
#pragma comment(lib, "opus_d.lib")
//#pragma comment(lib, "celt_d.lib")
//#pragma comment(lib, "silk_common_d.lib")
//#pragma comment(lib, "silk_float_d.lib")
#pragma comment(lib, "libogg_d.lib")
#else
#pragma comment(lib, "opus.lib")
//#pragma comment(lib, "celt.lib")
//#pragma comment(lib, "silk_common.lib")
//#pragma comment(lib, "silk_float.lib")
#pragma comment(lib, "libogg.lib")
#endif
#endif

//#define DEBUG_OUTPUT 1

AD_PLUGIN_API * adapi=NULL;

struct BUFFER {
	uint8 * data;
	uint32 len;
};

void ClearBuffer(BUFFER * buf) {
	if (buf->data) {
		zfree(buf->data);
	}
	memset(buf, 0, sizeof(BUFFER));
}

void AddBuffer(BUFFER * buf, uint8 * data, uint32 len) {
	buf->data = (uint8 *)zrealloc(buf->data, buf->len + len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
}

class Opus_Encoder: public AutoDJ_Encoder {
private:
	OpusEncoder * fHandle;
	int fInSample, fOutSample;
	int fOutChannels;
	BUFFER buffer;
	ogg_stream_state oss;
	ogg_packet op;
	ogg_page og;

	bool EncodeFromBuffer(bool finish);
	void deinitOpusObjects();

#if defined(DEBUG_OUTPUT)
	FILE * debugfp;
#endif

public:
	Opus_Encoder();
	virtual ~Opus_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();

};

Opus_Encoder::Opus_Encoder() {
	fHandle = NULL;
	fInSample = 0;
	//fake_stereo = false;
	memset(&buffer, 0, sizeof(buffer));
#if defined(DEBUG_OUTPUT)
	debugfp = NULL;
#endif
}


Opus_Encoder::~Opus_Encoder() {
}

void Opus_Encoder::deinitOpusObjects() {
	ogg_stream_clear(&oss);
	if (fHandle) {
		opus_encoder_destroy(fHandle);
		fHandle=NULL;
	}
}

bool is_opus_samplerate(int samplerate) {
	if (samplerate == 8000 || samplerate == 12000 || samplerate == 16000 || samplerate == 24000 || samplerate == 48000) {
		return true;
	}
	return false;
}

//const double opus_frame_sizes[] = { 0.06, 0.04, 0.02, 0.01, 0.00 };
const int opus_frame_sizes[] = { 60, 40, 20, 10, -1 };
bool Opus_Encoder::Init(int channels, int samplerate, float scale) {
	if (!is_opus_samplerate(samplerate)) {
		adapi->botapi->ib_printf(_("AutoDJ (opus_encoder) -> Samplerate must be 8000, 12000, 16000, 24000, or 48000!\n"));
		return false;
	}

	memset(&oss, 0, sizeof(oss));
	memset(&op, 0, sizeof(op));
	memset(&og, 0, sizeof(og));

	fOutChannels = adapi->GetOutputChannels();
	fOutSample = adapi->GetOutputSample();

	int err=0;
	fHandle = opus_encoder_create(samplerate, channels, OPUS_APPLICATION_AUDIO, &err);
	if (fHandle == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (opus_encoder) -> Error creating OpusEncoder handle! (%d)\n"), err);
		return false;
	}

	opus_encoder_ctl(fHandle, OPUS_SET_BITRATE(adapi->GetOutputBitrate() * 1000));

	fInSample = (samplerate / 100) * 6;

	ClearBuffer(&buffer);

	if (ogg_stream_init(&oss, AutoDJ_Rand()) != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (opus_encoder) -> Error initializing encoder. (ogg_stream_init())\n"));
		deinitOpusObjects();
		return false;
	}

	OpusHeader header;
	memset(&header, 0, sizeof(header));
	header.channels = header.nb_streams = channels;
	header.input_sample_rate = samplerate;
	header.gain = 0.0;
	header.preskip = 0;
	header.version = 1;
	header.nb_coupled = 0;
	header.nb_streams = 1;
	header.channel_mapping = 0;

    unsigned char header_data[100];
    int packet_size=opus_header_to_packet(&header, header_data, 100);
    op.packet=header_data;
    op.bytes=packet_size;
    op.b_o_s=1;
    op.e_o_s=0;
    op.granulepos=0;
    op.packetno=0;
    ogg_stream_packetin(&oss, &op);

	const char * opus_version=opus_get_version_string();
	adapi->botapi->ib_printf(_("AutoDJ (opus_encoder) -> Using libogg+%s\n"), opus_version);
  /*Vendor string should just be the encoder library,
    the ENCODER comment specifies the tool used.*/
	char *comments;
	int comments_length = 0;
	comment_init(&comments, &comments_length, opus_version);
	char ENCODER_string[256];
	snprintf(ENCODER_string, sizeof(ENCODER_string), "ShoutIRC.com RadioBot / AutoDJ v%s",adapi->botapi->GetVersionString());
	comment_add(&comments, &comments_length, "ENCODER=", ENCODER_string);

    op.packet=(unsigned char *)comments;
    op.bytes=comments_length;
    op.b_o_s=0;
    op.e_o_s=0;
    op.granulepos=0;
    op.packetno=1;
    ogg_stream_packetin(&oss, &op);

#if defined(DEBUG_OUTPUT)
	debugfp = fopen("debug.opus","wb");
#endif

	ogg_page page;
    while (ogg_stream_flush(&oss, &page)) {
		adapi->GetFeeder()->Send(page.header, page.header_len);
		adapi->GetFeeder()->Send(page.body, page.body_len);
#if defined(DEBUG_OUTPUT)
		fwrite(page.header, page.header_len, 1, debugfp);
		fwrite(page.body, page.body_len, 1, debugfp);
#endif
    }

	free(comments);

	return true;
}

bool Opus_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}

//BUFFER buffer = {NULL,0};

bool Opus_Encoder::EncodeFromBuffer(bool finish) {

	if (fHandle == NULL) { return true; }
	//opuslusEncConfiguration * conf = opuslusEncGetCurrentConfiguration(fHandle);
	//conf->inputFormat = FAAC_INPUT_16BIT;
	//faacEncSetConfiguration(fHandle, conf);

	bool ret = true;
	unsigned char buf[4000];// = (unsigned char *)zmalloc(fMaxBuffer);
	op.packet = buf;
	uint32 samples_per_frame = fInSample * fOutChannels;
	while ((buffer.len / 2) >= samples_per_frame && ret) {
		opus_int32 len=0;
	#ifdef WIN32
		__try {
	#endif
		len = opus_encode(fHandle, (opus_int16 *)buffer.data, fInSample, buf, sizeof(buf));
		memmove(buffer.data, buffer.data + (samples_per_frame*2), buffer.len - (samples_per_frame*2));
		buffer.len -= (samples_per_frame*2);
		buffer.data = (uint8 *)zrealloc(buffer.data, buffer.len);
	#ifdef WIN32
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			adapi->botapi->ib_printf(_("AutoDJ (opus_encoder) -> ERROR: Exception occured while encoding Opus Data! (%d / %s)\n"), len, opus_strerror(len));
			len = -1;
		}
	#endif
		if (len < 0) {
			adapi->botapi->ib_printf(_("AutoDJ (opus_encoder) -> Error encoding Opus Data! (%d / %s / %d / %d)\n"), len, opus_strerror(len), fInSample, samples_per_frame);
			ret = false;
		}
		if (len > 1) {
			op.packetno++;
			op.bytes = len;
			ogg_stream_packetin(&oss, &op);
			op.granulepos += fInSample * 48000/fOutSample;
		}
	}

	if (ret) {
		while (ret && ogg_stream_pageout(&oss, &og)) {
			ret = adapi->GetFeeder()->Send(og.header, og.header_len);
			if (ret) { ret = adapi->GetFeeder()->Send(og.body, og.body_len); }
	#if defined(DEBUG_OUTPUT)
			fwrite(og.header, og.header_len, 1, debugfp);
			fwrite(og.body, og.body_len, 1, debugfp);
	#endif
		}

		if (finish) {
			op.packetno++;
			op.bytes = 0;
			op.e_o_s = 1;
			ogg_stream_packetin(&oss, &op);
			while (ret && ogg_stream_flush(&oss, &og)) {
				ret = adapi->GetFeeder()->Send(og.header, og.header_len);
				if (ret) { ret = adapi->GetFeeder()->Send(og.body, og.body_len); }
		#if defined(DEBUG_OUTPUT)
				fwrite(og.header, og.header_len, 1, debugfp);
				fwrite(og.body, og.body_len, 1, debugfp);
		#endif
			}
		}
	}

	return ret;
}

bool Opus_Encoder::Encode(int samples, const short sample_buffer[]) {
	if (fHandle == NULL) { return true; }

	AddBuffer(&buffer, (uint8 *)sample_buffer, samples*2*fOutChannels);
	return EncodeFromBuffer(false);
}

void Opus_Encoder::Close() {
	EncodeFromBuffer(true);
	deinitOpusObjects();
#if defined(DEBUG_OUTPUT)
	if (debugfp) {
		fclose(debugfp);
		debugfp = NULL;
	}
#endif
	ClearBuffer(&buffer);
	fInSample  = 0;
	fOutChannels = 0;
}

AutoDJ_Encoder * opus_enc_create() { return new Opus_Encoder; }
void opus_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<Opus_Encoder *>(enc);
}

ENCODER opus_encoder = {
	"opus",
	"audio/ogg",
	2,

	opus_enc_create,
	opus_enc_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterEncoder(&opus_encoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Opus Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
