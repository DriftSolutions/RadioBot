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
#include <sndfile.h>

#define WAV_BUFFER_SIZE 524288

struct buffer {
  unsigned char const *start;
  uint32 length;
};

AD_PLUGIN_API * adapi=NULL;
SF_INFO info;
SNDFILE * sf;
READER_HANDLE * sff;
int32 frameno=0;
int32 sf_time_total=0;
int64 sf_time_start=0;

bool sf_is_my_file(const char * fn, const char * meta_type) {
	const char * ext = NULL;
	if (fn && (ext = strrchr(fn,'.')) ) {
		if (!stricmp(ext,".wav") || !stricmp(ext,".au") || !stricmp(ext,".snd") || !stricmp(ext,".paf") || !stricmp(ext,".voc")) {
			return true;
		}
	}
	return false;
}

bool sf_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td,0,sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }

#ifdef WIN32
	char * p = (char *)strrchr(fn, '\\');
#else
	char * p = (char *)strrchr(fn, '/');
#endif
	if (p) {
		sstrcpy(td->title, p+1);
	} else {
		sstrcpy(td->title, fn);
	}
	if ((p = strrchr(td->title, '.'))) {
		p[0]=0;
	}

	strtrim(td->title," ");
	strtrim(td->artist," ");

	if (songlen) {
		SF_INFO info2;
		memset(&info2, 0, sizeof(info2));
		SNDFILE * sf2 = sf_open(fn, SFM_READ, &info2);
		if (sf2) {
			sf_count_t tmp = info2.frames / info2.samplerate;
			*songlen = uint32(tmp);
			sf_close(sf2);
		}
	}

	return true;
}

class WAV_Decoder: public AutoDJ_Decoder {
private:
	SNDFILE * sf;
public:
	virtual ~WAV_Decoder();

	virtual bool Open(READER_HANDLE * fpp, int64 startpos);
	virtual bool Open_URL(const char * url, int64 startpos);

	virtual int64 GetPosition();
	virtual int32 Decode();
	virtual void Close();
};

WAV_Decoder::~WAV_Decoder(){}

int64 WAV_Decoder::GetPosition() {
	return sf ? sf_seek(sf, 0, SEEK_CUR):0;
}

bool WAV_Decoder::Open(READER_HANDLE * fnIn, int64 startpos) {
	sf = NULL;

	if (stricmp(fnIn->type, "file")) {
		return false;
	}

	memset(&info, 0, sizeof(info));
	sf = sf_open(fnIn->fn, SFM_READ, &info);

	if (!sf) {
		return false;
	}

	char buf[128];
	sf_command (NULL, SFC_GET_LIB_VERSION, buf, sizeof(buf));
	adapi->botapi->ib_printf(_("AutoDJ (sf_decoder) -> Using: %s\n"), buf);
	sf_command (sf, SFC_SET_SCALE_FLOAT_INT_READ, NULL, SF_TRUE);

	frameno = 0;
	int64 tmp = info.frames / info.samplerate;
	song_length = int32(tmp)*1000;
	sf_time_total = int32(tmp);
	sf_time_start = time(NULL);
	if (startpos) {
		sf_seek(sf, startpos, SEEK_SET);
	}
	adapi->botapi->ib_printf(_("AutoDJ (sf_decoder) -> Song Length: %d seconds\n"), sf_time_total);

	return adapi->GetDeck(deck)->SetInAudioParams(info.channels, info.samplerate);
}

bool WAV_Decoder::Open_URL(const char * fn, int64 startpos) {
	sf = NULL;

	if (access(fn, 0) != 0) {
		return false;
	}

	memset(&info, 0, sizeof(info));
	sf = sf_open(fn, SFM_READ, &info);

	if (!sf) {
		return false;
	}

	char buf[128];
	sf_command (NULL, SFC_GET_LIB_VERSION, buf, sizeof(buf));
	adapi->botapi->ib_printf(_("AutoDJ (sf_decoder) -> Using: %s\n"), buf);
	sf_command (sf, SFC_SET_SCALE_FLOAT_INT_READ, NULL, SF_TRUE);

	frameno = 0;
	int64 tmp = info.frames / info.samplerate;
	sf_time_total = int32(tmp);
	sf_time_start = time(NULL);
	if (startpos) {
		sf_seek(sf, startpos, SEEK_SET);
	}
	adapi->botapi->ib_printf(_("AutoDJ (sf_decoder) -> Song Length: %d seconds\n"), sf_time_total);

	return adapi->GetDeck(deck)->SetInAudioParams(info.channels, info.samplerate);
}


int32 WAV_Decoder::Decode() {
	short items[256];
	if (info.channels == 2) {
		uint32 cnt = uint32(sf_readf_short(sf, &items[0], 128));
		if (cnt > 0 && cnt <= 128) {
			adapi->GetDeck(deck)->AddSamples(&items[0], cnt);
			return 1;
		} else {
			return 0;
		}
	} else {
		uint32 cnt = uint32(sf_readf_short(sf, &items[0], 256)) ;
		if (cnt > 0 && cnt <= 256) {
			adapi->GetDeck(deck)->AddSamples(&items[0], cnt);
			return 1;
		} else {
			return 0;
		}
	}
}

void WAV_Decoder::Close() {
	adapi->botapi->ib_printf(_("AutoDJ (sf_decoder) -> sf_close()\n"));
	if (sf) {
		sf_close(sf);
		sf = NULL;
	}
	//adapi->Timing_Done();
}

AutoDJ_Decoder * sf_create() { return new WAV_Decoder; }
void sf_destroy(AutoDJ_Decoder * dec) { delete dynamic_cast<WAV_Decoder *>(dec); }

DECODER sf_decoder = {
	1,
	sf_is_my_file,
	sf_get_title_data,
	sf_create,
	sf_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterDecoder(&sf_decoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Waveform Decoder",

	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
