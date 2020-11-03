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

#include "autodj.h"
#include <math.h>

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif

bool ResamplerSpeex::Init(AUTODJ_DECKS ideck, int32 iChannels, int32 iinSampleRate, int32 ioutSampleRate) {
	Close();

	deck = ideck;
	numChannels = iChannels;
	inSampleRate = iinSampleRate;
	outSampleRate = ioutSampleRate;

	if (inSampleRate == outSampleRate) {
		api->ib_printf("AutoDJ (resampler_soxr) -> No resampling needed, samplerates are the same!\n");
		return false;
	}

	bool ret = true;
	int err = 0;
	ss = speex_resampler_init(numChannels, inSampleRate, outSampleRate, SPEEX_RESAMPLER_QUALITY_DEFAULT, &err);
	if (ss) {
		api->ib_printf2(pluginnum,_("AutoDJ (resampler_speex) -> [%s] Resampling %dhz audio to %dhz\n"), GetDeckName(deck), inSampleRate, outSampleRate);
	} else {
		api->ib_printf2(pluginnum,_("AutoDJ (resampler_speex) -> [%s] Error initializing resampler! (%s)\n"), GetDeckName(deck), speex_resampler_strerror(err));
		ret = false;
	}
	return ret;
}

void ResamplerSpeex::Close() {
	if (ss != NULL) {
		speex_resampler_destroy(ss);
		ss = NULL;
	}
}

int32 ResamplerSpeex::Resample(int32 samples, Audio_Buffer * bufIn, Audio_Buffer * bufOut) {
	if (ss == NULL) {
		return 0;
	}

	bool done = false;

	int err = 0;
	spx_uint32_t left = samples;
	spx_uint32_t sout = samples * numChannels;
	short * pIn = bufIn->buf;
	short * out = (short *)zmalloc(sout*sizeof(short));

	while (!done) {
		sout = samples;
		spx_uint32_t s = left;

		if ((err = speex_resampler_process_interleaved_int(ss, pIn, &s, out, &sout)) == 0) {
			left -= s;
			pIn += (s*numChannels);

			int ind = bufOut->len;
			bufOut->realloc(bufOut, bufOut->len + (sout*numChannels));
			//memcpy(((char *)buffer->buf)+(ind*sizeof(short)), out, sout*numChannels);
			for (unsigned int i=0; i < (sout*numChannels); i++) {
				bufOut->buf[ind+i] = out[i];
			}
			if (left == 0 && s == 0 && sout == 0) {
				done = true;
			}
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ (resampler_speex) -> [%s] Error resampling audio! (%s)\n"), GetDeckName(deck), speex_resampler_strerror(err));
			bufOut->reset(bufOut);
			break;
		}
	}

	int32 ret = bufOut->len / numChannels;
	zfree(out);
	return ret;
}
