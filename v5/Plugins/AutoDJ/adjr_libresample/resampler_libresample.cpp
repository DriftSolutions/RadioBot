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

AD_PLUGIN_API * adapi=NULL;

//#include <math.h>
#include "../../../../libresample/libresample.h"

class ResamplerLibResample: public Resampler {
private:
	void * ss[2];
public:
	ResamplerLibResample() { ss[0] = ss[1] = NULL; }
	~ResamplerLibResample() { Close(); }
	virtual bool Init(AUTODJ_DECKS deck, int32 numChannels, int32 inSampleRate, int32 outSampleRate);
	virtual int32 Resample(int32 samples, Audio_Buffer * bufIn, Audio_Buffer * bufOut);
	virtual void Close();
};

bool ResamplerLibResample::Init(AUTODJ_DECKS ideck, int32 iChannels, int32 iinSampleRate, int32 ioutSampleRate) {
	Close();

	deck = ideck;
	numChannels = iChannels;
	inSampleRate = iinSampleRate;
	outSampleRate = ioutSampleRate;

	if (inSampleRate == outSampleRate) {
		adapi->botapi->ib_printf2(adapi->GetPluginNum(), "AutoDJ (resampler_libresample) -> No resampling needed, samplerates are the same!\n");
		return false;
	}

	bool ret = true;
	double factor = outSampleRate;
	factor /= inSampleRate;
	ss[0] = resample_open(1, factor, factor);
	if (numChannels > 1) {
		ss[1] = resample_open(1, factor, factor);
	}
	adapi->botapi->ib_printf2(adapi->GetPluginNum(),_("AutoDJ (resampler_libresample) -> [%s] Resampling %dhz audio to %dhz\n"), adapi->GetDeckName(deck), inSampleRate, outSampleRate);
	return ret;
}

void ResamplerLibResample::Close() {
	if (ss[0] != NULL) {
		resample_close(ss[0]);
		ss[0] = NULL;
	}
	if (ss[1] != NULL) {
		resample_close(ss[1]);
		ss[1] = NULL;
	}
}

inline void short2float(short * s, float * f, uint32 num) {
	for (uint32 i=0; i < num; i++) {
		f[i] = float(s[i]) / 32767.0f;
	}
}

inline void float2short(float * f, short * s, uint32 num) {
	int tmp = 0;
	for (uint32 i=0; i < num; i++) {
		tmp = ((double)f[i] * 32767);// * DBL_EPSILON;
		if (tmp < -32767) { /* not -32768 */
			tmp = -32767;
        } else if (tmp > 32767) {
			tmp = 32767;
		}
		s[i] = (short)tmp;//f[i] * 32767;
	}
}

int32 ResamplerLibResample::Resample(int32 samples, Audio_Buffer * bufIn, Audio_Buffer * bufOut) {
	if (ss[0] == NULL) {
		return 0;
	}

	uint32 left=samples;

	float * f[2];
	f[0] = new float[samples];
	Audio_Buffer * bTmp[2];
	//float * fIn = f;
	if (numChannels == 1) {
		f[1] = NULL;
		short2float(bufIn->buf, f[0], samples);
	} else {
		bTmp[0] = adapi->AllocAudioBuffer();
		bTmp[1] = adapi->AllocAudioBuffer();
		f[1] = new float[samples];
		for (int i=0; i < samples; i++) {
			f[0][i] = double(bufIn->buf[i*2]) / 32767;
			f[1][i] = double(bufIn->buf[(i*2)+1]) / 32767;
		}
	}

	double factor = outSampleRate;
	factor /= inSampleRate;

	uint32 expected = samples * factor;
	float * fo = new float[expected];

	for (int i=0; i < numChannels; i++) {
		int fu=0, spos=0;
		//bool done = false;
		left = samples;
		for (;;) {
			/*
			if (spos > 0) {
				spos=spos;
			}
			*/
			int n = resample_process(ss[i], factor, &f[i][spos], left, 1, &fu, fo, expected);
			spos += fu;
			left -= fu;
			if (n > 0) {
				if (numChannels == 1) {
					int ind = bufOut->len;
					bufOut->realloc(bufOut, bufOut->len + n);
					float2short(fo, bufOut->buf+ind, n);
				} else {
					int ind = bTmp[i]->len;
					bTmp[i]->realloc(bTmp[i], bTmp[i]->len + n);
					float2short(fo, bTmp[i]->buf+ind, n);
				}
			}
			if (n < 0 || (n == 0 && spos == samples)) {
				break;
			}
		}
	}

	if (numChannels > 1) {
		if (bTmp[0]->len < bTmp[1]->len) {
			int o=bTmp[0]->len;
			bTmp[0]->realloc(bTmp[0], bTmp[1]->len);
			for (int i=o; i < bTmp[1]->len; i++) {
				bTmp[0]->buf[i] = bTmp[0]->buf[o-1];
			}
		}
		if (bTmp[1]->len < bTmp[0]->len) {
			int o=bTmp[1]->len;
			bTmp[1]->realloc(bTmp[1], bTmp[0]->len);
			for (int i=o; i < bTmp[0]->len; i++) {
				bTmp[1]->buf[i] = bTmp[1]->buf[o-1];
			}
		}
		bufOut->realloc(bufOut, bTmp[1]->len * numChannels);
		for (int i=0; i < bTmp[1]->len; i++) {
			bufOut->buf[i*2] = bTmp[0]->buf[i];
			bufOut->buf[(i*2)+1] = bTmp[1]->buf[i];
		}
		adapi->FreeAudioBuffer(bTmp[0]);
		adapi->FreeAudioBuffer(bTmp[1]);
	}

	delete [] fo;
	delete [] f[0];
	if (f[1]) { delete [] f[1]; }
	return bufOut->len / numChannels;
}

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	return true;
}

void quit() {
}

Resampler * create() { return new ResamplerLibResample; }
void destroy(Resampler * r) { delete dynamic_cast<ResamplerLibResample *>(r); }

AD_RESAMPLER_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Resampler: libresample",

	init,
	quit,

	create,
	destroy,

	NULL
};

PLUGIN_EXPORT AD_RESAMPLER_PLUGIN * GetADResampler() { return &plugin; }
