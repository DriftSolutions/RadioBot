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
#include <mmsystem.h>
#pragma comment(lib, "Winmm.lib")

AD_PLUGIN_API * adapi=NULL;

struct WAVE_QUEUE {
	WAVE_QUEUE *Next;
	WAVEHDR header;
};
typedef std::vector<WAVEHDR *> chunkList;

class Soundcard_Encoder: public AutoDJ_Encoder {
private:
	int inChannels, inSample;
	bool first_packet;
	time_t started;

public:
	HWAVEOUT hWaveOut;
	chunkList chunks;
	//WAVE_QUEUE *pFirst, *pLast;
	//WAVE_QUEUE *dFirst, *dLast;
	CRITICAL_SECTION cs;
	bool outOfSamples;

	Soundcard_Encoder();
	~Soundcard_Encoder();
	virtual bool Init(int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 num_samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	Soundcard_Encoder * enc = (Soundcard_Encoder *)dwInstance;
	if (uMsg == WOM_DONE && enc) {
		EnterCriticalSection(&enc->cs);
		for (chunkList::iterator x = enc->chunks.begin(); x != enc->chunks.end();) {
			if ((*x)->dwFlags & WHDR_DONE) {
				waveOutUnprepareHeader(enc->hWaveOut, *x, sizeof(WAVEHDR));
				zfree((*x)->lpData);
				zfree(*x);
				enc->chunks.erase(x);
				x = enc->chunks.begin();
			} else {
				x++;
			}
		}
		/*
		WAVE_QUEUE * Scan = enc->pFirst;
		WAVE_QUEUE * Last = NULL;
		while (Scan) {
			if (&Scan->header == (WAVEHDR *)dwParam1) {
				if (enc->dFirst) {
					enc->dLast->Next = Scan;
					enc->dLast = Scan;
				} else {
					enc->dFirst = enc->dLast = Scan;
				}
				Scan->Next = NULL;
			}
			Last = Scan;
			Scan = Scan->Next;
		}
		*/
		/*
		WAVE_QUEUE * Scan = enc->pFirst;
		if (Scan) {
			enc->pFirst = enc->pFirst->Next;
			if (enc->pFirst == NULL) {
				enc->pLast = NULL;
			}
			Scan->Next = NULL;

			if (enc->dLast) {
				enc->dLast->Next = Scan;
			} else {
				enc->dFirst = enc->dLast = Scan;
			}
			waveOutPrepareHeader(enc->hWaveOut, &Scan->header, sizeof(WAVEHDR));
			waveOutWrite(enc->hWaveOut, &Scan->header, sizeof(WAVEHDR));
		} else {
			enc->outOfSamples = true;
			//waveOutPause(enc->hWaveOut);
		}
		*/

		/*
		while (enc->dFirst && enc->dFirst->header.dwFlags & WHDR_DONE) {
			waveOutUnprepareHeader(enc->hWaveOut, &enc->dFirst->header, sizeof(WAVEHDR));
			WAVE_QUEUE * toDel = enc->dFirst;
			if (enc->dFirst == enc->dLast) {
				enc->dFirst = enc->dLast = NULL;
			} else {
				enc->dFirst = enc->dFirst->Next;
			}
			zfree(toDel->header.lpData);
			zfree(toDel);
		}
		*/
		LeaveCriticalSection(&enc->cs);

	}
}


Soundcard_Encoder::Soundcard_Encoder() {
	inChannels = inSample = 0;
	first_packet = false;
	outOfSamples = false;
	hWaveOut=NULL;
//	pFirst = pLast = NULL;
	//dFirst = dLast = NULL;
}

Soundcard_Encoder::~Soundcard_Encoder() {
	Close();
}

bool Soundcard_Encoder::Init(int channels, int samplerate, float scale) {
	inChannels = channels;// > 1 ? 2:1;
	inSample = samplerate;

	outOfSamples = false;
	first_packet = true;
	started = 0;

    WAVEFORMATEX wfx; /* look this up in your documentation */
//    MMRESULT result;/* for waveOut return values */

    // set up the WAVEFORMATEX structure. the structure describes the format of the audio.
    wfx.nSamplesPerSec = samplerate; /* sample rate */
    wfx.wBitsPerSample = 16; /* sample size */
    wfx.nChannels = channels; /* channels*/

	/*
    * WAVEFORMATEX also has other fields which need filling.
    * as long as the three fields above are filled this should
    * work for any PCM (pulse code modulation) format.
    */
    wfx.cbSize = 0; /* size of _extra_ info */
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

	InitializeCriticalSection(&cs);
	//if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, (DWORD_PTR)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
	if(waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
		hWaveOut = NULL;
		DeleteCriticalSection(&cs);
        adapi->botapi->ib_printf("AutoDJ (soundcard output) -> Could not open WAVE_MAPPER device!\n");
        return false;
    }

	/*
	DWORD vol;
	waveOutGetVolume(hWaveOut, &vol);
	adapi->botapi->ib_printf("AutoDJ (soundcard output) -> Volume L: %d R: %d!\n", LOWORD(vol), HIWORD(vol));
	vol = (0x8000 << 16) | 0x8000;
	waveOutSetVolume(hWaveOut, vol);
	waveOutPause(hWaveOut);
	*/
	return true;
}

bool Soundcard_Encoder::Raw(unsigned char * data, int32 len) {
	//return adapi->GetFeeder()->Send(data, len);
	return false;
}

bool Soundcard_Encoder::Encode(int32 num_samples, const short buffer[]) {

	/*
	WAVE_QUEUE * Scan = (WAVE_QUEUE *)zmalloc(sizeof(WAVE_QUEUE));
	memset(Scan, 0, sizeof(WAVE_QUEUE));
	uint32 size = sizeof(short)*num_samples;
	//Scan->header.dwFlags = WHDR_PREPARED;
	Scan->header.dwBufferLength = size;
	Scan->header.lpData = (LPSTR)zmalloc(size);
	memcpy(Scan->header.lpData, buffer, size);
	*/
	WAVEHDR * header = znew(WAVEHDR);
	memset(header, 0, sizeof(WAVEHDR));
	uint32 size = sizeof(short)*num_samples*inChannels;
	//Scan->header.dwFlags = WHDR_PREPARED;
	header->dwBufferLength = size;
	header->lpData = (LPSTR)zmalloc(size);
	memcpy(header->lpData, buffer, size);

	EnterCriticalSection(&cs);
	chunks.push_back(header);
	waveOutPrepareHeader(hWaveOut, header, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, header, sizeof(WAVEHDR));

	for (chunkList::iterator x = chunks.begin(); x != chunks.end();) {
		if ((*x)->dwFlags & WHDR_DONE) {
			waveOutUnprepareHeader(hWaveOut, *x, sizeof(WAVEHDR));
			zfree((*x)->lpData);
			zfree(*x);
			chunks.erase(x);
			x = chunks.begin();
		} else {
			x++;
		}
	}

	/*
	if (pLast) {
		pLast->Next = Scan;
	} else {
		pFirst = pLast = Scan;
	}
	*/
	LeaveCriticalSection(&cs);
	/*
	if (outOfSamples) {
		started = 0;
		first_packet == true;
		outOfSamples = false;
	}
	if (outOfSamples || first_packet == true) {
		if (started == 0) {
			started = time(NULL);
		} else if ((time(NULL) - started) > 4) {
			//waveOutMessage(hWaveOut, WOM_DONE, 0, 0);
			waveOutProc(hWaveOut, WOM_DONE, (DWORD_PTR)this, 0, 0);
			first_packet = false;
		}
	} else if (outOfSamples && chunks.size()) {
		outOfSamples = false;
		waveOutProc(hWaveOut, WOM_DONE, (DWORD_PTR)this, 0, 0);
		//waveOutRestart(hWaveOut);
	}
	*/

	if (first_packet == true) {
		if (started == 0) {
			started = time(NULL);
		} else if ((time(NULL) - started) > 4) {
			waveOutRestart(hWaveOut);
			//waveOutMessage(hWaveOut, WOM_DONE, 0, 0);
			//waveOutProc(hWaveOut, WOM_DONE, (DWORD_PTR)this, 0, 0);
			first_packet = false;
		}
	}

	return true;
}

void Soundcard_Encoder::Close() {
	if (hWaveOut) {
		EnterCriticalSection(&cs);
		waveOutReset(hWaveOut);

		while (chunks.size()) {
			for (chunkList::iterator x = chunks.begin(); x != chunks.end(); x++) {
				if ((*x)->dwFlags & WHDR_DONE) {
					waveOutUnprepareHeader(hWaveOut, *x, sizeof(WAVEHDR));
					zfree((*x)->lpData);
					zfree(*x);
					chunks.erase(x);
					break;
				}
			}
		}
		/*
		WAVE_QUEUE * Scan = dFirst;
		while (Scan) {
			WAVE_QUEUE * toDel = Scan;
			Scan = Scan->Next;
			waveOutUnprepareHeader(hWaveOut, &toDel->header, sizeof(WAVEHDR));
			zfree(toDel->header.lpData);
			zfree(toDel);
		}
		*/

		while (waveOutClose(hWaveOut) == WAVERR_STILLPLAYING){
			adapi->botapi->safe_sleep_milli(50);
		}
		hWaveOut = NULL;

		/*
		Scan = pFirst;
		while (Scan) {
			WAVE_QUEUE * toDel = Scan;
			Scan = Scan->Next;
			zfree(toDel->header.lpData);
			zfree(toDel);
		}
		*/

		LeaveCriticalSection(&cs);
		DeleteCriticalSection(&cs);
	}
}

AutoDJ_Encoder * soundcard_create() { return new Soundcard_Encoder; }
void soundcard_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<Soundcard_Encoder *>(enc);
}

ENCODER soundcard_encoder = {
	"soundcard",
	"audio/pcm",
	100,

	soundcard_create,
	soundcard_destroy,
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterEncoder(&soundcard_encoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Soundcard Output",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
