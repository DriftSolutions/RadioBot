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
#include <lame/lame.h>

AD_PLUGIN_API * adapi=NULL;

class MP3_Encoder: public AutoDJ_Encoder {
private:
	lame_global_flags *gfp;
	int inChannels;//, inSampleRate;
public:
	MP3_Encoder();
	virtual ~MP3_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

MP3_Encoder::MP3_Encoder() {
	gfp = NULL;
}

MP3_Encoder::~MP3_Encoder() {
}

bool MP3_Encoder::Init(int channels, int samplerate, float scale) {
	inChannels = channels;// == 1 ? 1:2;
	//inSampleRate = samplerate;
	char buf[1024]={0};
	gfp = lame_init();
	lame_version_t lvp;
	get_lame_version_numerical(&lvp);

	adapi->botapi->ib_printf(_("AutoDJ (mp3_encoder) -> Using LAME %d.%d\n"), lvp.major, lvp.minor);
	//api->botapi->ib_printf("AutoDJ (mp3_encoder) -> [stream-%02d] Using LAME %s\n",num, get_lame_version());
	sstrcpy(buf, _("AutoDJ (mp3_encoder) -> Encoding to "));

	//set what will be fed into LAME
	lame_set_num_channels(gfp, channels);
	lame_set_in_samplerate(gfp, samplerate);

	//and what it should be outputting
	lame_set_out_samplerate(gfp, adapi->GetOutputSample());
	//lame_set_bWriteVbrTag(gfp,0);
	lame_set_copyright(gfp, 1);
	lame_set_scale(gfp, scale);
	lame_set_brate(gfp, adapi->GetOutputBitrate());
	/*
 quality=0..9.  0=best (very slow).  9=worst.
  recommended:  2     near-best quality, not too slow
                5     good quality, fast
                7     ok quality, really fast
	*/
	lame_set_quality(gfp, 4);
	int mode = adapi->GetOutputChannels() > 1 ? JOINT_STEREO:MONO;

	DS_CONFIG_SECTION * sec = adapi->botapi->config->GetConfigSection(NULL, "AutoDJ");
	if (sec) {
		sec = adapi->botapi->config->GetConfigSection(sec, "MP3_Encoder");
		if (sec) {

			int tmp = adapi->botapi->config->GetConfigSectionLong(sec, "Quality");
			if (tmp < 0 || tmp > 9) {
				tmp = 4;
			}
			lame_set_quality(gfp,tmp);

			if (channels > 1) {
				char * smode = adapi->botapi->config->GetConfigSectionValue(sec, "Mode");
				if (smode) {
					if (smode[0] == 'j' || smode[0] == 'J') {
						mode = JOINT_STEREO;
					}
					if (smode[0] == 's' || smode[0] == 'S') {
						mode = STEREO;
					}
				}
			}

			if (adapi->botapi->config->GetConfigSectionLong(sec, "VBR") > 0) {
				lame_set_VBR(gfp, vbr_default);
				adapi->botapi->config->GetConfigSectionLong(sec, "VBR_Quality");
				if (tmp < 0 || tmp > 9) {
					tmp = 5;
				}
				lame_set_VBR_q(gfp, tmp);
				lame_set_VBR_mean_bitrate_kbps(gfp, adapi->GetOutputBitrate());
				tmp = adapi->botapi->config->GetConfigSectionLong(sec, "MinBitrate");
				if (tmp > 0) {
					if (tmp > adapi->GetOutputBitrate()) { tmp = adapi->GetOutputBitrate(); }
					lame_set_VBR_min_bitrate_kbps(gfp, tmp);
				} else {
					lame_set_VBR_min_bitrate_kbps(gfp, int(adapi->GetOutputBitrate()*0.25));
				}
				tmp = adapi->botapi->config->GetConfigSectionLong(sec, "MaxBitrate");
				if (tmp > 0) {
					if (tmp < adapi->GetOutputBitrate()) { tmp = adapi->GetOutputBitrate(); }
					lame_set_VBR_max_bitrate_kbps(gfp, tmp);
				} else {
					lame_set_VBR_max_bitrate_kbps(gfp, int(adapi->GetOutputBitrate()*1.25));
				}
			}
		}
	}

	switch(mode) {
		case JOINT_STEREO:
			lame_set_mode(gfp, JOINT_STEREO);
			strcat(buf, _("J-Stereo/"));
			break;
		case STEREO:
			lame_set_mode(gfp, STEREO);
			strcat(buf, _("Stereo/"));
			break;
		default:
			lame_set_mode(gfp, MONO);
			strcat(buf, _("Mono/"));
			break;
	}
	sprintf(((char *)buf)+strlen(buf), _("%dHz, "), adapi->GetOutputSample());

	if (lame_get_VBR(gfp) == vbr_off) {
		sprintf(((char *)buf)+strlen(buf), _("CBR %dkbps"), adapi->GetOutputBitrate());
	} else {
		sprintf(((char *)buf)+strlen(buf), _("VBR (%d,%d,%d)kbps"), lame_get_VBR_min_bitrate_kbps(gfp), lame_get_VBR_mean_bitrate_kbps(gfp), lame_get_VBR_max_bitrate_kbps(gfp));
	}
	adapi->botapi->ib_printf("%s\n", buf);

#if defined(DEBUG)
	//lame_print_config(gfp);
#endif
	if (lame_init_params(gfp) == -1) {
		adapi->botapi->ib_printf(_("AutoDJ (mp3_encoder) -> lame_init_params() has returned ERROR!\n"));
		lame_close(gfp);
		gfp=NULL;
		return false;
	}
	return true;
}

bool MP3_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}

bool MP3_Encoder::Encode(int samples, const short buffer[]) {
	if (gfp == NULL) { return true; }
	long bsize = (long)((1.25 * samples) + 7200);
	unsigned char * buf = (unsigned char *)zmalloc(bsize);
	//long len = lame_encode_buffer_int(gfp,left,right,samples,buf,bsize);
	long len=0;

#ifdef WIN32
	__try {
#endif
	if (inChannels == 1) { // lame_encode_buffer_interleaved doesn't seem to be able to handle mono in any way, as an input or output
		len = lame_encode_buffer(gfp,(short *)buffer,(short *)buffer,samples,buf,bsize);
	} else {
		len = lame_encode_buffer_interleaved(gfp,(short *)buffer,samples,buf,bsize);
	}
#ifdef WIN32
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		adapi->botapi->ib_printf(_("ERROR: Exception occured while encoding MP3 Data!\n"));
		len = -1;
	}
#endif

	if (len < 0) {
		zfree(buf);
		return false;
	}

	bool ret = adapi->GetFeeder()->Send(buf,len);
	zfree(buf);
	return ret;
}

void MP3_Encoder::Close() {
	if (gfp) {
		unsigned char mp3buf[7200];
		int len = 0;
#ifdef WIN32
		__try {
#endif
		len = lame_encode_flush(gfp,mp3buf,sizeof(mp3buf));
#ifdef WIN32
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			adapi->botapi->ib_printf(_("ERROR: Exception occured while encoding MP3 Data!\n"));
			len = 0;
		}
#endif
		if (len > 0) {
			adapi->GetFeeder()->Send(mp3buf,len);
		}
		lame_close(gfp);
		gfp=NULL;
	}
}

AutoDJ_Encoder * mp3_enc_create() { return new MP3_Encoder; }
void mp3_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<MP3_Encoder *>(enc);
}

ENCODER mp3_encoder = {
	"mp3",
	"audio/mpeg",
	0,

	mp3_enc_create,
	mp3_enc_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterEncoder(&mp3_encoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"MP3 Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
