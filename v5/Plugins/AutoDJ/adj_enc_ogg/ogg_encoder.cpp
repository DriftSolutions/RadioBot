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

#include "ogg_codec.h"

AD_PLUGIN_API * adapi=NULL;

class OGG_Encoder: public AutoDJ_Encoder {
private:
	//lame_global_flags *gfp;
	bool opened;
	vorbis_info vi;
	vorbis_dsp_state vds;
	vorbis_block vb;
	vorbis_comment vc;
	ogg_stream_state oss;
	int num_samples;

	int inChannels, inSample;

	bool outputBlocks();
	void deinitVorbisObjects();
public:
	OGG_Encoder();
	virtual ~OGG_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

OGG_Encoder::OGG_Encoder() {
	memset(&vi, 0, sizeof(vi));
	memset(&vds, 0, sizeof(vds));
	memset(&vb, 0, sizeof(vb));
	memset(&vc, 0, sizeof(vc));
	memset(&oss, 0, sizeof(oss));
	//gfp = NULL;
	opened = false;
	inChannels = inSample = 0;
}

OGG_Encoder::~OGG_Encoder() {
}

void OGG_Encoder::deinitVorbisObjects() {
	ogg_stream_clear(&oss);
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vds);
    vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);
}

bool OGG_Encoder::Init(int channels, int samplerate, float scale) {
	memset(&vi, 0, sizeof(vi));
	memset(&vds, 0, sizeof(vds));
	memset(&vb, 0, sizeof(vb));
	memset(&vc, 0, sizeof(vc));
	memset(&oss, 0, sizeof(oss));

	num_samples = 0;
	inChannels = channels;// == 1 ? 1:2;
	inSample = samplerate;

	vorbis_info_init(&vi);
	int n=0;

	//adapi->GetOutputChannels(server) == 1 ? 1:2
	if ((n = vorbis_encode_init(&vi, inChannels, samplerate, -1, adapi->GetOutputBitrate()*1000, -1)) != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error initializing encoder. (vorbis_encode_init() -> %d (%d,%d))\n"), n, adapi->GetOutputBitrate(), adapi->GetOutputBitrate()*1000);
		deinitVorbisObjects();
		return false;
	}

	if (vorbis_analysis_init(&vds, &vi) != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error initializing encoder. (vorbis_analysis_init())\n"));
		deinitVorbisObjects();
		return false;
	}

	if (vorbis_block_init(&vds, &vb) != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error initializing encoder. (vorbis_block_init())\n"));
		deinitVorbisObjects();
		return false;
	}

	if (ogg_stream_init(&oss, AutoDJ_Rand()) != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error initializing encoder. (ogg_stream_init())\n"));
		deinitVorbisObjects();
		return false;
	}

	vorbis_comment_init(&vc);
    ogg_packet header;
    ogg_packet commentHeader;
    ogg_packet codeHeader;
	memset(&header, 0, sizeof(header));
	memset(&commentHeader, 0, sizeof(header));
	memset(&codeHeader, 0, sizeof(header));

	if (vorbis_analysis_headerout(&vds, &vc, &header, &commentHeader, &codeHeader) != 0) {
		adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error initializing encoder. (vorbis_analysis_headerout())\n"));
		deinitVorbisObjects();
		return false;
	}

    ogg_stream_packetin(&oss, &header);
    ogg_stream_packetin(&oss, &commentHeader);
    ogg_stream_packetin(&oss, &codeHeader);

    ogg_page page;
    while (ogg_stream_flush(&oss, &page)) {
		adapi->GetFeeder()->Send(page.header, page.header_len);
		adapi->GetFeeder()->Send(page.body, page.body_len);
    }

	opened = true;
	return true;
}

bool OGG_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}

bool OGG_Encoder::outputBlocks() {
    while (vorbis_analysis_blockout(&vds, &vb) == 1) {
        ogg_packet pack;
        ogg_page page;
		memset(&pack, 0, sizeof(pack));
		memset(&page, 0, sizeof(page));

		if (vorbis_analysis(&vb, NULL) == 0) {
			if (vorbis_bitrate_addblock(&vb) == 0) {
				while (vorbis_bitrate_flushpacket(&vds, &pack)) {
					if (ogg_stream_packetin(&oss, &pack) == 0) {
						while ((num_samples >= inSample) ? ogg_stream_flush(&oss, &page) : ogg_stream_pageout(&oss, &page)) {
							num_samples = 0;
							if (!adapi->GetFeeder()->Send(page.header, page.header_len) || !adapi->GetFeeder()->Send(page.body, page.body_len)) {
								return false;
							}
						}
					} else {
						adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error in ogg_stream_packetin! (ogg_stream_packetin())\n"));
					}
				}
			} else {
				adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error adding vorbis block! (vorbis_bitrate_addblock())\n"));
			}
		} else {
			adapi->botapi->ib_printf(_("AutoDJ (ogg_encoder) -> Error in vorbis analysis! (vorbis_analysis())\n"));
		}
    }
	return true;
}

bool OGG_Encoder::Encode(int samples, const short buf2[]) {
	if (samples > 0) {
		signed char * buf = (signed char *)buf2;

		//buffer/encode data
		float ** buffer = vorbis_analysis_buffer(&vds, samples);

		for(int i=0; i < samples; i++) {
			for(int j=0; j < inChannels; j++) {
				buffer[j][i]= ((buf[2*(i*inChannels + j) + 1]<<8) | (0x00ff&(int)buf[2*(i*inChannels + j)]))/32768.f;
			}
		}

		vorbis_analysis_wrote(&vds, samples);
		num_samples += samples;

		return outputBlocks();
	}
	return true;
}

void OGG_Encoder::Close() {
	if (opened) {
		vorbis_analysis_wrote(&vds, 0);
		outputBlocks();
		deinitVorbisObjects();
		opened = false;
	}
}

AutoDJ_Encoder * ogg_enc_create() { return new OGG_Encoder; }
void ogg_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<OGG_Encoder *>(enc);
}

ENCODER ogg_encoder = {
	"oggvorbis",
	"audio/ogg",
	2,

	ogg_enc_create,
	ogg_enc_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterDecoder(&ogg_decoder);
	adapi->RegisterEncoder(&ogg_encoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"OGG Decoder/Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
