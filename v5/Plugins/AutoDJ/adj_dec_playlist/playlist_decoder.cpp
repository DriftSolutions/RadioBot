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

bool playlist_is_my_file(const char * fn, const char * mime_type) {
	if (fn && strrchr(fn,'.') && (!stricmp(strrchr(fn,'.'),".m3u") || !stricmp(strrchr(fn,'.'),".pls") || !stricmp(strrchr(fn,'.'),".m3u8"))) {//
		return true;
	}
	return false;
}

bool playlist_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }
	return false;
}

class PL_SORT_QUEUE {
public:
	PL_SORT_QUEUE() {
		used = false;
	}
	QUEUE * que;
	bool used;
};

class Playlist_Decoder: public AutoDJ_Decoder {
private:
	int playlistType(const char * fn) {
		const char * ext = strrchr(fn,'.');
		if (ext) {
			if (!stricmp(ext,".m3u") || !stricmp(ext,".m3u8")) {
				return 1;
			}
			if (!stricmp(ext,".pls")) {
				return 2;
			}
		}
		return 0;
	}

	int loadM3U(char * buf) {
		int cnt = 0;
		bool random = false;
		std::vector<PL_SORT_QUEUE> songs;
		PL_SORT_QUEUE pl;
		char * p2 = NULL;
		char * line = strtok_r(buf, "\n\r", &p2);
		while (line) {
			char * p = adapi->strdup(line);
			strtrim(p);
			if (p[0] != '#' && p[0] != 0) {
				QUEUE * ret = adapi->AllocQueue();
				char * fn = nopath(p);
				if (fn) {
					ret->fn = adapi->strdup(fn);
					char * q = adapi->strdup(p);
					char * r = strrchr(q, '/');
					if (r == NULL || strrchr(q, '\\') > r) {
						r = strrchr(q, '\\');
					}
					if (r != NULL) { *(++r) = 0; }
					ret->path = adapi->strdup(q);
					adapi->free(q);
				} else {
					ret->fn = adapi->strdup(p);
					ret->path = adapi->strdup("");
				}
				ret->ID = adapi->FileID(p);
				ret->mTime = 0;

				pl.que = ret;
				songs.push_back(pl);
				cnt++;
			} else if (!stricmp(p, "#RANDOM")) {
				if (!random) {
					adapi->botapi->ib_printf2(adapi->GetPluginNum(), _("AutoDJ (playlist_decoder) -> Playlist randomization ON...\n"));
				}
				random = true;
			}
			adapi->free(p);
			line = strtok_r(NULL, "\n\r", &p2);
		}
		if (random) {
			uint32 left = songs.size();
			while (left) {
				uint32 x = adapi->botapi->genrand_range(0, songs.size() - 1);
				if (!songs[x].used) {
					adapi->AddToPlayQueue(songs[x].que);
					songs[x].used = true;
					left--;
				}
			}
		} else {
			for (std::vector<PL_SORT_QUEUE>::const_iterator x = songs.begin(); x != songs.end(); x++) {
				adapi->AddToPlayQueue(x->que);
			}
		}
		songs.clear();
		adapi->botapi->ib_printf2(adapi->GetPluginNum(), _("AutoDJ (playlist_decoder) -> Loaded %d songs from M3U playlist...\n"), cnt);
		return cnt;
	}

	int loadPLS(char * buf) {
		char tmp[4096],tmp2[64];
		int count = atoi(Get_INI_String_Memory(buf, "playlist", "NumberOfEntries", tmp, sizeof(tmp), "0"));
		bool random = (stricmp(Get_INI_String_Memory(buf, "playlist", "Random", tmp, sizeof(tmp), "false"),"true") == 0) ? true:false;
		if (random) {
			adapi->botapi->ib_printf2(adapi->GetPluginNum(), _("AutoDJ (playlist_decoder) -> Playlist randomization ON...\n"));
		}
		std::vector<PL_SORT_QUEUE> songs;
		PL_SORT_QUEUE pl;
		int cnt = 0;

		for (int i=1; i <= count; i++) {
			sprintf(tmp2, "File%d", i);
			char * p = Get_INI_String_Memory(buf, "playlist", tmp2, tmp, sizeof(tmp)-1);
			if (p && p[0]) {
				QUEUE * ret = adapi->AllocQueue();
				char * fn = nopath(p);
				if (fn) {
					ret->fn = adapi->strdup(fn);
					char * q = adapi->strdup(p);
					char * r = strrchr(q, '/');
					if (r == NULL || strrchr(q, '\\') > r) {
						r = strrchr(q, '\\');
					}
					if (r != NULL) { *(++r) = 0; }
					ret->path = adapi->strdup(q);
					adapi->free(q);
				} else {
					ret->fn = adapi->strdup(p);
					ret->path = adapi->strdup("");
				}
				ret->ID = adapi->FileID(p);
				ret->mTime = 0;
				sprintf(tmp2, "Length%d", i);
				ret->songlen = atoi(Get_INI_String_Memory(buf, "playlist", tmp2, tmp, sizeof(tmp)-1, "0"));
				sprintf(tmp2, "Title%d", i);
				p = Get_INI_String_Memory(buf, "playlist", tmp2, tmp, sizeof(tmp)-1);
				if (p && p[0]) {
					ret->meta = (TITLE_DATA *)adapi->malloc(sizeof(TITLE_DATA));
					memset(ret->meta, 0, sizeof(TITLE_DATA));
					sstrcpy(ret->meta->title, p);
				}
				pl.que = ret;
				songs.push_back(pl);
				cnt++;
			}
		}
		if (random) {
			uint32 left = songs.size();
			while (left) {
				uint32 x = adapi->botapi->genrand_range(0, songs.size() - 1);
				if (!songs[x].used) {
					adapi->AddToPlayQueue(songs[x].que);
					songs[x].used = true;
					left--;
				}
			}
		} else {
			for (std::vector<PL_SORT_QUEUE>::const_iterator x = songs.begin(); x != songs.end(); x++) {
				adapi->AddToPlayQueue(x->que);
			}
		}
		songs.clear();
		adapi->botapi->ib_printf2(adapi->GetPluginNum(), _("AutoDJ (playlist_decoder) -> Loaded %d songs from PLS playlist...\n"), cnt);
		return cnt;
	}

public:
	virtual ~Playlist_Decoder() {

	}

	virtual bool Open(READER_HANDLE * fp, int64 startpos) {
		string sbuf;
		char tmp[4096];
		int32 n;
		while (!fp->eof(fp) && (n = fp->read(tmp, sizeof(tmp)-1, fp)) > 0) {
			tmp[n] = 0;
			sbuf += tmp;
		}

		char * buf = adapi->strdup(sbuf.c_str());
		sbuf.clear();
		int type = playlistType(fp->fn);
		switch (type) {
			case 1:
				loadM3U(buf);
				break;
			case 2:
				loadPLS(buf);
				break;
			default:
				adapi->botapi->ib_printf2(adapi->GetPluginNum(), _("AutoDJ (playlist_decoder) -> Unknown playlist type for file: %s\n"), fp->fn);
				break;
		}
		adapi->free(buf);
		return true;
	}

	virtual bool Open_URL(const char * url, int64 startpos) {
		return false;
	}

	virtual int64 GetPosition() {
		return 0;
	}

	virtual DECODE_RETURN Decode() {
		return AD_DECODE_DONE;
	}

	virtual bool WantTitleUpdate() {
		return false;
	}

	virtual void Close() {
		adapi->botapi->ib_printf(_("AutoDJ (playlist_decoder) -> Close()\n"));
	}
};

AutoDJ_Decoder * playlist_create() { return new Playlist_Decoder; }
void playlist_destroy(AutoDJ_Decoder * dec) { delete dynamic_cast<Playlist_Decoder *>(dec); }

DECODER playlist_decoder = {
	0,
	playlist_is_my_file,
	playlist_get_title_data,
	playlist_create,
	playlist_destroy
};

bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	adapi->RegisterDecoder(&playlist_decoder);
	return true;
};

void quit() {
};

AD_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Playlist Decoder",

	init,
	NULL,
	quit
};

PLUGIN_EXPORT AD_PLUGIN * GetADPlugin() { return &plugin; }
