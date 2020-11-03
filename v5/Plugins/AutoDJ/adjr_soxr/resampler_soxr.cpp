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
#if defined(WIN32)
#define SOXR_DLL
#elif defined(HAVE_VIS)
#define SOXR_VISIBILITY
#endif
#include <soxr.h>

class ResamplerSoxr: public Resampler {
private:
	soxr_t ss;
public:
	ResamplerSoxr() { ss = NULL; }
	~ResamplerSoxr() { Close(); }
	virtual bool Init(AUTODJ_DECKS deck, int32 numChannels, int32 inSampleRate, int32 outSampleRate);
	virtual int32 Resample(int32 samples, Audio_Buffer * bufIn, Audio_Buffer * bufOut);
	virtual void Close();
};

bool ResamplerSoxr::Init(AUTODJ_DECKS ideck, int32 iChannels, int32 iinSampleRate, int32 ioutSampleRate) {
	Close();

	deck = ideck;
	numChannels = iChannels;
	inSampleRate = iinSampleRate;
	outSampleRate = ioutSampleRate;

	if (inSampleRate == outSampleRate) {
		adapi->botapi->ib_printf2(adapi->GetPluginNum(), "AutoDJ (resampler_soxr) -> No resampling needed, samplerates are the same!\n");
		return false;
	}

	bool ret = true;
	soxr_error_t err;
	soxr_io_spec_t io;
	memset(&io, 0, sizeof(io));
	io.itype = SOXR_INT16_I;
	io.otype = SOXR_INT16_I;
	io.scale = 1;
	ss = soxr_create(inSampleRate, outSampleRate, numChannels, &err, &io, NULL, NULL);
	if (ss) {
		adapi->botapi->ib_printf2(adapi->GetPluginNum(),_("AutoDJ (resampler_soxr) -> [%s] Resampling %dhz audio to %dhz\n"), adapi->GetDeckName(deck), inSampleRate, outSampleRate);
	} else {
		adapi->botapi->ib_printf2(adapi->GetPluginNum(),_("AutoDJ (resampler_soxr) -> [%s] Error initializing resampler! (%s)\n"), adapi->GetDeckName(deck), err);
		ret = false;
	}
	return ret;
}

void ResamplerSoxr::Close() {
	if (ss != NULL) {
		soxr_delete(ss);
		ss = NULL;
	}
}

int32 ResamplerSoxr::Resample(int32 samples, Audio_Buffer * bufIn, Audio_Buffer * bufOut) {
	if (ss == NULL) {
		return 0;
	}
	int32 ret = samples;

	bool done = false;

	soxr_error_t err = NULL;
	size_t left = samples;
	size_t sout = samples * numChannels;
	short * pIn = bufIn->buf;
	short * out = (short *)zmalloc(sout*sizeof(short));

	while (!done) {
		size_t num_output;
		size_t num_used;

		if ((err = soxr_process((soxr_t)ss, pIn, left, &num_used, out, samples, &num_output)) == NULL) {
			left -= num_used;
			pIn += (num_used*numChannels);

			if (num_output > 0) {
				int ind = bufOut->len;
				bufOut->realloc(bufOut, bufOut->len + (num_output*numChannels));
				for (size_t i=0; i < (num_output*numChannels); i++) {
					bufOut->buf[ind+i] = out[i];
				}
			} else {
				left=left;
			}
			if (left == 0) {
				done = true;
			}
		} else {
			adapi->botapi->ib_printf2(adapi->GetPluginNum(),_("AutoDJ (resampler_soxr) -> [%s] Error resampling audio! (%s)\n"), adapi->GetDeckName(deck), err);
			bufOut->reset(bufOut);
			break;
		}
	}

	ret = bufOut->len / numChannels;
	zfree(out);
	return ret;
}

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	return true;
}

void quit() {
}

Resampler * create() { return new ResamplerSoxr; }
void destroy(Resampler * r) { delete dynamic_cast<ResamplerSoxr *>(r); }

AD_RESAMPLER_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Resampler: soxr",

	init,
	quit,

	create,
	destroy,

	NULL
};

PLUGIN_EXPORT AD_RESAMPLER_PLUGIN * GetADResampler() { return &plugin; }
