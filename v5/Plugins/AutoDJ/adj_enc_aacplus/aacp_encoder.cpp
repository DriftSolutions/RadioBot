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
#include <aacplus.h>

/*
#if defined(WIN32)
#if defined(DEBUG)
#pragma comment(lib, "libaacplus_d.lib")
#else
#pragma comment(lib, "libaacplus.lib")
#endif
#endif
*/

AD_PLUGIN_API * adapi=NULL;
DL_HANDLE aacpDL = NULL;
typedef aacplusEncConfiguration * (*aacplusEncGetCurrentConfigurationType)(aacplusEncHandle hEncoder);
typedef int (*aacplusEncSetConfigurationType)(aacplusEncHandle hEncoder, aacplusEncConfiguration *cfg);
typedef aacplusEncHandle (*aacplusEncOpenType)(unsigned long sampleRate, unsigned int numChannels, unsigned long *inputSamples, unsigned long *maxOutputBytes);
typedef int (*aacplusEncEncodeType)(aacplusEncHandle hEncoder, int32_t *inputBuffer, unsigned int samplesInput, unsigned char *outputBuffer, unsigned int bufferSize);
typedef int (*aacplusEncCloseType)(aacplusEncHandle hEncoder);

struct AACP_DEF {
	aacplusEncGetCurrentConfigurationType aacplusEncGetCurrentConfiguration;
	aacplusEncSetConfigurationType aacplusEncSetConfiguration;
	aacplusEncOpenType aacplusEncOpen;
	aacplusEncEncodeType aacplusEncEncode;
	aacplusEncCloseType aacplusEncClose;
};
AACP_DEF aacp;

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

class AACPlus_Encoder: public AutoDJ_Encoder {
private:
	aacplusEncHandle fHandle;
	unsigned long fInSample, fMaxBuffer;
	int fOutChannels;
	//bool fake_stereo;
	int server;
	BUFFER buffer;
	bool EncodeFromBuffer(bool finish);

public:
	AACPlus_Encoder();
	virtual ~AACPlus_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

AACPlus_Encoder::AACPlus_Encoder() {
	fHandle = NULL;
	fInSample = fMaxBuffer = fOutChannels = 0;
	//fake_stereo = false;
	memset(&buffer, 0, sizeof(buffer));
}


AACPlus_Encoder::~AACPlus_Encoder() {
}


bool AACPlus_Encoder::Init(int channels, int samplerate, float scale) {

	//int och = fOutChannels = adapi->GetOutputChannels();// == 1 ? 1:2;
	//fake_stereo = false;
	/*
	if (channels > 2) { channels = 2; }
	int och = fOutChannels = adapi->GetOutputChannels();// == 1 ? 1:2;
	if (channels < och) {
		channels = och;
		//fake_stereo = true;
	} else if (channels > adapi->GetOutputChannels()) {
		channels = adapi->GetOutputChannels();
	}
	*/

	fHandle = aacp.aacplusEncOpen(samplerate, channels, &fInSample, &fMaxBuffer);
	if (fHandle == NULL) {
		return false;
	}

	aacplusEncConfiguration * conf = aacp.aacplusEncGetCurrentConfiguration(fHandle);
	conf->bitRate = adapi->GetOutputBitrate() * 1000;// / och;
	conf->bandWidth = 0;
	conf->nChannelsIn = conf->nChannelsOut = fOutChannels = channels;
	conf->inputFormat = AACPLUS_INPUT_16BIT;
	conf->outputFormat = 1;
	conf->sampleRate = samplerate;
	conf->nSamplesPerFrame = 1024;
	aacp.aacplusEncSetConfiguration(fHandle, conf);

	ClearBuffer(&buffer);
	return true;
}

bool AACPlus_Encoder::Raw(unsigned char * data, int32 len) {
	return adapi->GetFeeder()->Send(data, len);
}

//BUFFER buffer = {NULL,0};

bool AACPlus_Encoder::EncodeFromBuffer(bool finish) {

	if (fHandle == NULL) { return true; }
	//aacplusEncConfiguration * conf = aacplusEncGetCurrentConfiguration(fHandle);
	//conf->inputFormat = FAAC_INPUT_16BIT;
	//faacEncSetConfiguration(fHandle, conf);

	bool ret = true;
	while ((buffer.len / 2) >= fInSample && ret) {
		unsigned char * buf = (unsigned char *)zmalloc(fMaxBuffer);
		int len=0;
	#ifdef WIN32
		__try {
	#endif
		len = aacp.aacplusEncEncode(fHandle, (int32_t *)buffer.data, fInSample, buf, fMaxBuffer);
		memmove(buffer.data, buffer.data + (fInSample*2), buffer.len - (fInSample*2));
		buffer.len -= (fInSample*2);
		buffer.data = (uint8 *)zrealloc(buffer.data, buffer.len);
	#ifdef WIN32
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			adapi->botapi->ib_printf(_("AutoDJ (aacp_encoder) -> ERROR: Exception occured while encoding AAC+ Data!\n"));
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
			len = aacp.aacplusEncEncode(fHandle, NULL, 0, buf, fMaxBuffer);
		#ifdef WIN32
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				adapi->botapi->ib_printf("AutoDJ (aacp_encoder) -> ERROR: Exception occured while encoding AAC+ Data!\n");
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

bool AACPlus_Encoder::Encode(int samples, const short sample_buffer[]) {
	if (fHandle == NULL) { return true; }

	AddBuffer(&buffer, (uint8 *)sample_buffer, samples*2*fOutChannels);
	return EncodeFromBuffer(false);
}

void AACPlus_Encoder::Close() {
	if (fHandle) {
		EncodeFromBuffer(true);
		aacp.aacplusEncClose(fHandle);
		fHandle=NULL;
	}
	ClearBuffer(&buffer);
	fMaxBuffer = 0;
	fInSample  = 0;
}

AutoDJ_Encoder * aacp_enc_create() { return new AACPlus_Encoder; }
void aacp_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<AACPlus_Encoder *>(enc);
}

ENCODER aacp_encoder = {
	"aacp",
	"audio/aacp",
	1,

	aacp_enc_create,
	aacp_enc_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	memset(&aacp, 0, sizeof(aacp));
	aacpDL = NULL;

#ifdef WIN32
	#ifdef DEBUGxxx
		aacpDL = DL_Open("libaacplus_d.dll");
	#else
		aacpDL = DL_Open("libaacplus.dll");
	#endif
#else
	adapi->botapi->ib_printf("AutoDJ (aacp_encoder) -> Loading libaacplus.so ...\n");
	aacpDL = DL_Open("libaacplus.so");
	if (aacpDL == NULL) {
		WHEREIS_RESULTS * res = WhereIs("libaacplus.so*");
		if (res) {
			for (int i=0; i < res->nCount && aacpDL == NULL; i++) {
				adapi->botapi->ib_printf("AutoDJ (aacp_encoder) -> Attempting %s ...\n", res->sResults[i]);
				aacpDL = DL_Open(res->sResults[i]);
			}
			WhereIs_FreeResults(res);
		}
	}
#endif
	if (aacpDL) {
		aacp.aacplusEncClose = (aacplusEncCloseType)DL_GetAddress(aacpDL, "aacplusEncClose");
		aacp.aacplusEncEncode = (aacplusEncEncodeType)DL_GetAddress(aacpDL, "aacplusEncEncode");
		aacp.aacplusEncGetCurrentConfiguration = (aacplusEncGetCurrentConfigurationType)DL_GetAddress(aacpDL, "aacplusEncGetCurrentConfiguration");
		aacp.aacplusEncOpen = (aacplusEncOpenType)DL_GetAddress(aacpDL, "aacplusEncOpen");
		aacp.aacplusEncSetConfiguration = (aacplusEncSetConfigurationType)DL_GetAddress(aacpDL, "aacplusEncSetConfiguration");

		if (aacp.aacplusEncClose == NULL || aacp.aacplusEncEncode == NULL || aacp.aacplusEncGetCurrentConfiguration == NULL || aacp.aacplusEncOpen == NULL || aacp.aacplusEncSetConfiguration == NULL) {
			adapi->botapi->ib_printf(_("AutoDJ (aacp_encoder) -> Could not resolve all symbols in libaacplus, unloading... (%s)\n"), DL_LastError());
			DL_Close(aacpDL);
			aacpDL = NULL;
			return false;
		}
	} else {
		adapi->botapi->ib_printf(_("AutoDJ (aacp_encoder) -> Could not load libaacplus, unloading... (%s)\n"), DL_LastError());
		return false;
	}

	adapi->RegisterEncoder(&aacp_encoder);
	return true;
};

void quit() {
	if (aacpDL) {
		aacpDL = NULL;
	}
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"AAC+ Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
