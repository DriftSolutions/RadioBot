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
#include <faac.h>

AD_PLUGIN_API * adapi=NULL;

/*
This format equals to an array of 32 bit integers (int) in all systems.
Full 32 bit value range is permitted. However the least significant bits will be ignored by
the devices. For example if this format is used with 24 bit devices then the value will be
divided by 256 before playback. 16 bit devices will discard the 16 least significant bits
(by dividing the value by 65536). Recording works in opposite way. The least significant bits
will be zeroes.
*/


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

class AAC_Encoder: public AutoDJ_Encoder {
private:
	faacEncHandle fHandle;
	unsigned long fInSample, fMaxBuffer;
	int fOutChannels;
	int server;
	BUFFER buffer;
	bool EncodeFromBuffer(bool finish);

public:
	AAC_Encoder();
	virtual ~AAC_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

AAC_Encoder::AAC_Encoder() {
	fHandle = NULL;
	fInSample = fMaxBuffer = fOutChannels = 0;
	memset(&buffer, 0, sizeof(buffer));
}


AAC_Encoder::~AAC_Encoder() {
}


bool AAC_Encoder::Init(int channels, int samplerate, float scale) {

	int och = fOutChannels = adapi->GetOutputChannels();// == 1 ? 1:2;
	/*
	if (channels < och) {
		channels = och;
	} else if (channels > api->GetOutputChannels()) {
		channels = 1;
	}
	*/

	fHandle = faacEncOpen(samplerate, channels, &fInSample, &fMaxBuffer);
	if (fHandle == NULL) {
		return false;
	}

	//char buf[1024]={0};

	char *id_str=NULL, *copy_str=NULL;
	faacEncGetVersion(&id_str, &copy_str);

	adapi->botapi->ib_printf(_("AutoDJ -> Using FAAC %s\n"),id_str);
	//GetOutputSample
	faacEncConfigurationPtr conf = faacEncGetCurrentConfiguration(fHandle);
	conf->aacObjectType = MAIN;
	conf->bitRate = adapi->GetOutputBitrate() * 1000 / och;
	conf->mpegVersion = MPEG4;//4
	conf->shortctl = SHORTCTL_NORMAL;
	conf->inputFormat = FAAC_INPUT_32BIT;
	conf->outputFormat = 1;
	conf->useLfe = 0;
	conf->useTns = 1;
	conf->allowMidside = 1;
	faacEncSetConfiguration(fHandle, conf);

	ClearBuffer(&buffer);
	return true;
}

bool AAC_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}

//BUFFER buffer = {NULL,0};

bool AAC_Encoder::EncodeFromBuffer(bool finish) {

	if (fHandle == NULL) { return true; }
	faacEncConfigurationPtr conf = faacEncGetCurrentConfiguration(fHandle);
	conf->inputFormat = FAAC_INPUT_16BIT;
	faacEncSetConfiguration(fHandle, conf);

	bool ret = true;
	while ((buffer.len / 2) >= fInSample && ret) {
		unsigned char * buf = (unsigned char *)zmalloc(fMaxBuffer);
		int len=0;
	#ifdef WIN32
		__try {
	#endif
		len = faacEncEncode(fHandle, (int32_t *)buffer.data, fInSample, buf, fMaxBuffer);
		memmove(buffer.data, buffer.data + (fInSample*2), buffer.len - (fInSample*2));
		buffer.len -= (fInSample*2);
		buffer.data = (uint8 *)zrealloc(buffer.data, buffer.len);
	#ifdef WIN32
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			adapi->botapi->ib_printf(_("ERROR: Exception occured while encoding AAC Data!\n"));
			len = -1;
		}
	#endif
		if (len < 0) {
			ret = false;
		}
		if (len > 0) {
			ret = adapi->GetFeeder()->Send(buf,len);
		}
		zfree(buf);
	}

	if (finish) {
		unsigned char * buf = (unsigned char *)zmalloc(fMaxBuffer);
		int len=1;
		while (ret && len > 0) {
		#ifdef WIN32
			__try {
		#endif
			len = faacEncEncode(fHandle, NULL, 0, buf, fMaxBuffer);
		#ifdef WIN32
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				adapi->botapi->ib_printf("ERROR: Exception occured while encoding AAC Data!\n");
				len = 0;
			}
		#endif
			if (len < 0) {
				ret = false;
			}
			if (len > 0) {
				ret = adapi->GetFeeder()->Send(buf,len);
			}
		}
		zfree(buf);
	}

	return ret;
}

bool AAC_Encoder::Encode(int samples, const short sample_buffer[]) {
	if (fHandle == NULL) { return true; }

	AddBuffer(&buffer, (uint8 *)sample_buffer, samples*2*fOutChannels);
	return EncodeFromBuffer(false);

	//adapi->botapi->ib_printf("AutoDJ (aac_encoder) -> This encoder does not support short interleaved encoding!\n");
	//return false;
}

void AAC_Encoder::Close() {
	if (fHandle) {
		EncodeFromBuffer(true);
		faacEncClose(fHandle);
		fHandle=NULL;
	}
	ClearBuffer(&buffer);
	fMaxBuffer = 0;
	fInSample  = 0;
}

AutoDJ_Encoder * aac_enc_create() { return new AAC_Encoder; }
void aac_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<AAC_Encoder *>(enc);
}

ENCODER aac_encoder = {
	"aac",
	"audio/aac",
	50,

	aac_enc_create,
	aac_enc_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterEncoder(&aac_encoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"AAC Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
