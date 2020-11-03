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
#if defined(DEBUG)
#include <assert.h>
#endif
#include <math.h>

DL_HANDLE hDSP=NULL;
AUTODJ_DSP * dspPlugin=NULL;

bool InitMixer();
void CloseMixer();

MIXER Mixer = {
	InitMixer,
	CloseMixer
};

MIXER * GetMixer() { return &Mixer; }

bool LoadDeck(int deck) {
	while (!ad_config.Decks[deck]->isLoaded()) {
		AutoMutex(QueueMutex);
		QUEUE * next = ad_config.nextSong;
		QUEUE * Scan = GetNextSong();
		if (Scan == NULL) {
			api->ib_printf2(pluginnum,"AutoDJ (mixer) -> Error getting song to play!\n");
			return false;
		}

		if (next == NULL && ad_config.nextSong == NULL && ad_config.Queue != NULL && stristr(Scan->fn, "adjintro") == NULL) {
			char buf[MAX_PATH];
			sstrcpy(buf, Scan->fn);
			strlcat(buf, ".adjintro.mp3", sizeof(buf));
			//api->ib_printf2(pluginnum,"AutoDJ (mixer) -> Searching for intro file: %s -> %s\n", Scan->fn, buf);
			QUEUE * intro = ad_config.Queue->FindFile(buf);
			if (intro) {
				//api->ib_printf2(pluginnum,"AutoDJ (mixer) -> Found!\n");
				ad_config.nextSong = Scan;
				Scan = intro;
			}
		}

		char buf[MAX_PATH];
		snprintf(buf, sizeof(buf), "%s%s", Scan->path, Scan->fn);
		ad_config.Decks[deck]->LoadFile(buf, Scan, Scan->meta);
		FreeQueue(Scan);
	}
	return true;
}

void SwapActiveDeck() {
	Mixer.curDeck = (Mixer.curDeck == DECK_A) ? DECK_B:DECK_A;
	Mixer.othDeck = (Mixer.curDeck == DECK_A) ? DECK_B:DECK_A;
}

void UnloadDecks() {
	if (ad_config.Decks[DECK_A]->isLoaded()) {
		ad_config.Decks[DECK_A]->Stop();
	}
	if (ad_config.Decks[DECK_B]->isLoaded()) {
		ad_config.Decks[DECK_B]->Stop();
	}
}

void UnloadActiveDeck() {
	if (ad_config.Decks[Mixer.curDeck]->isLoaded()) {
		ad_config.Decks[Mixer.curDeck]->Stop();
	}
}

int crossfade_samples_per_step = 1;
int crossfade_until_next_step = 1;
double crossfade_current = 1.0;
double crossfade_remove_per_step = 0;

//#define CROSSFADE_TEST

bool Encode() {
	if (ad_config.next && ad_config.Decks[Mixer.curDeck]->isLoaded()) {
		UnloadActiveDeck();
		ad_config.next = false;
		SwapActiveDeck();
	}

	if (ad_config.countdown_now) {
		UnloadActiveDeck();
		return true;
	}

	if (!ad_config.Decks[Mixer.curDeck]->isLoaded() && ad_config.Decks[Mixer.curDeck]->GetBufferedSamples() == 0) {
		if (!LoadDeck(Mixer.curDeck)) {
			api->ib_printf2(pluginnum,"AutoDJ [%s] -> Error loading deck! Will try again...\n", GetDeckName(Mixer.curDeck));
			return false;
		}
	}
	if (ad_config.Decks[Mixer.curDeck]->isLoaded() && !ad_config.Decks[Mixer.curDeck]->isPlaying() && ad_config.Decks[Mixer.curDeck]->GetBufferedSamples() == 0) {
		ad_config.cursong = ad_config.Decks[Mixer.curDeck]->getSong();
		ad_config.Decks[Mixer.curDeck]->Play();
	}

#if defined(DEBUG)
	if (!ad_config.Options.EnableCrossfade && ad_config.Decks[Mixer.curDeck]->isLoaded() && ad_config.Decks[Mixer.othDeck]->isLoaded()) {
		assert(0);
	}
#endif

#ifdef CROSSFADE_TEST
	if (!ad_config.Decks[Mixer.curDeck]->isLoaded()) {
		LoadDeck(Mixer.curDeck);
	}
	if (!ad_config.Decks[Mixer.othDeck]->isLoaded()) {
		LoadDeck(Mixer.othDeck);
	}
	if (!ad_config.Decks[Mixer.curDeck]->isPlaying()) {
		ad_config.Decks[Mixer.curDeck]->Play();
	}
	if (!ad_config.Decks[Mixer.othDeck]->isPlaying()) {
		ad_config.Decks[Mixer.othDeck]->Play();
	}
#endif

	//int chans = ad_config.Server.Channels >= 2 ? 2:1;
	//int32 onesec = ad_config.Server.Sample * chans;
	int32 samples = 0;
	for (int i=0; i < NUM_DECKS; i++) {
		if (ad_config.Decks[i]->isLoaded() && !ad_config.Decks[i]->isPaused()) {
			if (ad_config.Decks[i]->GetBufferedSamples() < ad_config.Server.Sample) {
				ad_config.Decks[i]->Decode(ad_config.Server.Sample);
				if (ad_config.Decks[i]->GetBufferedSamples() <= 0) {
					ad_config.Decks[i]->Stop();
					SwapActiveDeck();
					continue;
				}
			}
			//printf("x: %d, y: %d\n", Mixer.abuffers[i]->len, onesec);
			int nsamples = ad_config.Decks[i]->GetBufferedSamples();
			if (samples == 0 || nsamples < samples) {
				samples = nsamples;
			}
		}
	}

	if (samples > ad_config.Server.Sample) {
		samples = ad_config.Server.Sample;
	}

	short * buf = new short[samples*ad_config.Server.Channels];
	memset(buf, 0, sizeof(short)*samples*ad_config.Server.Channels);
	short * bufc[NUM_DECKS];

	int len = samples*ad_config.Server.Channels;
	for (int i=0; i < NUM_DECKS; i++) {
		if (ad_config.Decks[i]->GetBufferedSamples() > 0) {
			bufc[i] = new short[len];
			ad_config.Decks[i]->GetSamples(bufc[i], samples);
		} else {
			bufc[i] = NULL;
		}
	}

	//bool ret = true;
	/*
	double per = 1.0;
	if (bufc[Mixer.curDeck] && bufc[Mixer.othDeck]) {
		per = 0.50;
	}
	for (int i=0; i < NUM_DECKS; i++) {
		if (bufc[i]) {
			for (int j=0; j < (samples*ad_config.Server.Channels); j++) {
				buf[j] += bufc[i][j] * per * ad_config.Options.Volume;
			}
			delete [] bufc[i];
		}
	}
	*/

	bool in_fade = false;
	if (bufc[Mixer.curDeck] && bufc[Mixer.othDeck]) {
		in_fade = true;
	} else {
		crossfade_current = 1.0;
	}
	if (in_fade) {
		double chan0 = (Mixer.curDeck == 0) ? crossfade_current:1.0-crossfade_current;
		double chan1 = (Mixer.curDeck == 1) ? crossfade_current:1.0-crossfade_current;
		for (int j=0; j < (samples*ad_config.Server.Channels); j++) {
			buf[j] += bufc[0][j] * chan0 * ad_config.Options.Volume;
			buf[j] += bufc[1][j] * chan1 * ad_config.Options.Volume;
			if ((j % ad_config.Server.Channels) == 0) {
				crossfade_until_next_step--;
				if (crossfade_until_next_step <= 0) {
#ifdef CROSSFADE_TEST
					crossfade_samples_per_step = 2646;
					crossfade_remove_per_step = 1.0/50;
#endif
					crossfade_current -= crossfade_remove_per_step;
#ifdef CROSSFADE_TEST
					if (crossfade_current < 0.01) { crossfade_current = 1.0; }
#else
					if (crossfade_current < 0.01) { crossfade_current = 0.01; }
#endif
					chan0 = (Mixer.curDeck == 0) ? crossfade_current:1.0-crossfade_current;
					chan1 = (Mixer.curDeck == 1) ? crossfade_current:1.0-crossfade_current;
					crossfade_until_next_step = crossfade_samples_per_step;
					//api->ib_printf("current: %f\n", crossfade_current);
				}
			}
		}
		for (int i=0; i < NUM_DECKS; i++) {
			if (bufc[i]) {
				delete [] bufc[i];
			}
		}
	} else {
		for (int i=0; i < NUM_DECKS; i++) {
			if (bufc[i]) {
				for (int j=0; j < (samples*ad_config.Server.Channels); j++) {
					buf[j] += bufc[i][j] * ad_config.Options.Volume;
				}
				delete [] bufc[i];
			}
		}
	}

	if (dspPlugin) {
		samples = dspPlugin->modify_samples(samples, buf, ad_config.Server.Channels, ad_config.Server.Sample);
	}

	timer_add(&Mixer.timer, samples);

	if (ad_config.Options.EnableCrossfade && ad_config.Decks[Mixer.curDeck]->isPlaying() && ad_config.Decks[Mixer.curDeck]->getLength() >= ad_config.Options.CrossfadeMinDuration && !ad_config.countdown && ad_config.playing && ad_config.connected && !ad_config.shutdown_now) {
		int64 len = ad_config.Decks[Mixer.curDeck]->getLength();
		int64 len2 = ad_config.Decks[Mixer.curDeck]->getOutputTime();
		int64 diff = len - len2;
#if defined(DEBUG)
		//api->ib_printf2(pluginnum,"AutoDJ (mixer) -> Len: "I64FMT", Len2: "I64FMT", Diff: "I64FMT"\n", len, len2, diff);
#endif
		if (diff <= ad_config.Options.CrossfadeLength && diff > 0) {
			if (!ad_config.Decks[Mixer.othDeck]->isLoaded()) {
				api->ib_printf2(pluginnum,"AutoDJ (mixer) -> Loading other deck for crossfading...\n");
				LoadDeck(Mixer.othDeck);
			}
			if (ad_config.Decks[Mixer.othDeck]->isLoaded() && !ad_config.Decks[Mixer.othDeck]->isPlaying() && ad_config.Decks[Mixer.othDeck]->getLength() >= ad_config.Options.CrossfadeMinDuration) {
				api->ib_printf2(pluginnum,"AutoDJ (mixer) -> Crossfading...\n");
				ad_config.Decks[Mixer.othDeck]->Play();
				int steps = 50;
				crossfade_samples_per_step = ((ad_config.Server.Sample/1000)*diff) / 50;
				crossfade_until_next_step = crossfade_samples_per_step;
				crossfade_current = 1.0;
				crossfade_remove_per_step = 1.0/steps;
				steps=steps;
			}
		}
	}

	bool ret = ad_config.Encoder->Encode(samples, buf);
	delete [] buf;
	if (!ret) {
		api->ib_printf2(pluginnum,_("AutoDJ (mixer) -> Endoder/Feeder error, reconnecting to SS and reinitializing encoder/feeder...\n"));
		if (api->irc) { api->irc->LogToChan(_("AutoDJ -> Endoder/Feeder error, reconnecting to SS and reinitializing encoder/feeder...")); }
	}
	return ret;
}

bool encode_raw(unsigned char * data, int32 len) {

	bool ret = true;
	if (ad_config.Feeder) {
		if (!ad_config.Feeder->Send(data, len)) {
			ret = false;
		}
	}

	return ret;
}

uint64 getOutputTime() { return timer_value(&Mixer.timer); }

bool MixerStartup() {

//	Mixer.encode_signed_int = encode_signed_int;
//	Mixer.encode_short = encode_short;
//	Mixer.encode_short_interleaved = encode_short_interleaved;
	Mixer.encode = Encode;
	Mixer.raw = encode_raw;
	Mixer.getOutputTime = getOutputTime;

	if (strlen(ad_config.Options.DSPPlugin)) {
		char buf[512];
		sstrcpy(buf, ad_config.Options.DSPPlugin);
		str_replaceA(buf, sizeof(buf), "plugins/dsp_", "plugins/AutoDJ/dsp_");
		str_replaceA(buf, sizeof(buf), "plugins\\dsp_", "plugins\\AutoDJ\\dsp_");

		char * fn = nopath(ad_config.Options.DSPPlugin);
		hDSP = DL_Open(buf);
		if (hDSP) {
			GetAD_DSP_Type get = (GetAD_DSP_Type)DL_GetAddress(hDSP, "GetAD_DSP");
			if (get) {
				dspPlugin = get();
				if (dspPlugin) {
					if (dspPlugin->version == DSP_VERSION) {
						dspPlugin->hInstance = hDSP;
						if (dspPlugin->init) {
							if (dspPlugin->init(ad_config.Server.Channels, ad_config.Server.Sample)) {
								api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s: SUCCESS\n"), fn);
							} else {
								api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s: FAILED! (DSP init() returned false)\n"), fn);
								dspPlugin = NULL;
							}
						} else {
							api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s: SUCCESS\n"), fn);
						}
					} else {
						api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s: FAILED! (DSP version is not compatible with this AutoDJ build)\n"), fn);
						dspPlugin = NULL;
					}
				} else {
					api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s: FAILED! (GetAD_DSP() returned NULL)\n"), fn);
				}
			} else {
				api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s: FAILED! (DL_GetAddress returned NULL) (%s)\n"), fn, DL_LastError());
			}
			if (dspPlugin == NULL) {
				DL_Close(hDSP);
				hDSP = NULL;
			}
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to load plugin %s: FAILED! (DL_Open returned NULL) (%s)\n"), fn, DL_LastError());
		}
	}

	Mixer.curDeck = DECK_A;
	Mixer.othDeck = DECK_B;
	timer_init(&Mixer.timer, ad_config.Server.Sample);

	return true;
}

void MixerShutdown() {
	if (hDSP) {
		if (dspPlugin && dspPlugin->quit) {
			dspPlugin->quit();
		}
		DL_Close(hDSP);
		hDSP = NULL;
		dspPlugin = NULL;
	}
	UnloadDecks();
}

bool InitMixer() {
	timer_zero(&Mixer.timer);
	return ad_config.Encoder->Init(ad_config.Server.Channels, ad_config.Server.Sample, 1);
}

void CloseMixer() {
	//UnloadDecks();
	ad_config.Encoder->Close();
	//wipe existing sample buffers
}
