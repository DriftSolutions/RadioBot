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
#include "build.h"

/*
#if defined(WIN32)

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif


#include <wchar.h>
#include <sapi.h>
#include <sphelper.h>
#endif
*/

TT_DEFINE_THREAD(PlayThread);
//THREADTYPE PlaylistThread(VOID *lpData);
TT_DEFINE_THREAD(UpdateTitle);
TT_DEFINE_THREAD(Watchdog);
int AutoDJ_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);
int AutoDJ_YT_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

int pluginnum=0;
BOTAPI_DEF * api=NULL;

AD_CONFIG ad_config;
AD_CONFIG * GetADConfig() { return &ad_config; }
//unsigned long numplays=0;

int32 songs_until_promo = 0;
int32 songs_until_voice = 0;
int songs_until_next_playlist = 0;
int current_playlist=0;

QUEUE *pQue=NULL; // main play queue
QUEUE *rQue=NULL; // request queue
//QUEUE *prFirst=NULL,*prLast=NULL;
//QUEUE *prCur = NULL;

Titus_TimedMutex QueueMutex;

ArtistListType artistList;
RequestListType requestList;
RequestUserListType requestUserList;
RequestUserListType requestArtistList;

READER readers[] = {
	{ read_file_is_my_file, read_file_open },
	{ read_stream_is_my_file, read_stream_open },
#if !defined(WIN32) && defined(BLAHBLAHBLAH)
	{ read_mms_is_my_file, read_mms_open },
#endif
	{ NULL, NULL }
};

FEEDER feeders[] = {
	{ sc_create, sc_destroy, SS_TYPE_SHOUTCAST, "Shoutcast" },
	{ ultravox_create, ultravox_destroy, SS_TYPE_SHOUTCAST2, "Shoutcast2" },
	{ ic_create, ic_destroy, SS_TYPE_ICECAST, "Icecast2" },
	{ steamcast_create, steamcast_destroy, SS_TYPE_STEAMCAST, "Steamcast" },
	{ sp2p_create, sp2p_destroy, SS_TYPE_SP2P, "StreamerP2P" },
	{ NULL, NULL, 0, NULL }
};

bool MatchesNoPlayFilter(const char * str) {
	StrTokenizer * st = new StrTokenizer(ad_config.Options.NoPlayFilters, ',');
	int num = st->NumTok();
	for (int i=1; i <= num; i++) {
		char * pat = st->GetSingleTok(i);
		if (strlen(pat)) {
			if (wildicmp(pat, str)) {
				st->FreeString(pat);
				delete st;
				return true;
				break;
			}
		}
		st->FreeString(pat);
	}
	delete st;
	return false;
}

bool MatchFilter(const QUEUE * Scan, TIMER * Timer, const char * pat) {
	AutoMutex(QueueMutex);

	if (Timer->pat_type == TIMER_PAT_TYPE_FILENAME) {
		//botib_printf("Completed Pattern: %s\n", pat);
		//botLogToChan(buf);
		if (wildicmp_multi(pat,Scan->fn,"|,")) {
			return true;
		}
	} else if (Timer->pat_type == TIMER_PAT_TYPE_DIRECTORY) {
		//botib_printf("Completed Pattern: %s\n", pat);
		//botLogToChan(buf);
		if (wildicmp_multi(pat,Scan->path,"|,")) {
			return true;
		}
	} else if (Timer->pat_type == TIMER_PAT_TYPE_REQ_COUNT) {
		if (pat[0] == '>') {
			int rc = atoi(pat + 1);
			if (Scan->req_count > rc) {
				return true;
			}
		} else if (pat[0] == '=') {
			int rc = atoi(pat + 1);
			if (Scan->req_count == rc) {
				return true;
			}
		}
		return false;
	} else if (Scan->meta) {
		char * str = NULL;
		switch(Timer->pat_type) {
			case TIMER_PAT_TYPE_ARTIST:
				str = Scan->meta->artist;
				break;
			case TIMER_PAT_TYPE_ALBUM:
				str = Scan->meta->album;
				break;
			case TIMER_PAT_TYPE_GENRE:
				str = Scan->meta->genre;
				break;
			case TIMER_PAT_TYPE_YEAR:{
					if (Scan->meta->year > 0) {
						int32 iMin = 0, iMax = 0;
						if (api->textfunc->get_range(pat, &iMin, &iMax)) {
							if (Scan->meta->year >= iMin && Scan->meta->year <= iMax) {
								return true;
							}
						}
					}
					return false;
				}
				break;
			default:
				str = Scan->meta->title;
				break;
		}
		if (wildicmp_multi(pat,str,"|,")) {
			api->ib_printf(_("Matched Pattern: %s -> %s\n"), pat, str);
			return true;
		}
	}

	return false;
}

bool AllowSongPlayback(const QUEUE * Scan, time_t tme, const char * pat) {
	char buf[1024];
	sprintf(buf, "%s%s", Scan->path, Scan->fn);

	AutoMutex(QueueMutex);

	if (ad_config.Filter && tme < ad_config.Filter->extra && pat) {
		if (!MatchFilter(Scan, ad_config.Filter, pat)) {
			return false;
		}
	} else {
		//NoPlayFilters don't count during Filter'ed playback
		if (MatchesNoPlayFilter(buf)) {
			api->ib_printf2(pluginnum,_("AutoDJ -> %s blocked by NoPlayFilters...\n"), buf);
			return false;
		}
	}

	if (ad_config.Options.MaxSongDuration) {
		if (Scan->songlen > ad_config.Options.MaxSongDuration) {
			api->ib_printf2(pluginnum,_("AutoDJ -> Skipping Song: %s (%d seconds, MaxSongDuration: %d)\n"),Scan->fn,Scan->songlen,ad_config.Options.MaxSongDuration);
			return false;
		}
	}

	if (ad_config.Options.NoRepeat > 0) {
		int64 lp = tme - Scan->lastPlayed;
		if (lp < ad_config.Options.NoRepeat) {
			api->ib_printf2(pluginnum,_("AutoDJ -> Skipping Song (by Song): %s (last played " I64FMT " secs ago, NoRepeat = %u)\n"),Scan->fn,lp,ad_config.Options.NoRepeat);
			return false;
		}
	}

	if (ad_config.Options.NoRepeatArtist != 0 && Scan->meta && Scan->meta->artist[0]) {
		for (ArtistListType::iterator s = artistList.begin(); s != artistList.end(); ) {
			if (s->second <= (time(NULL)-ad_config.Options.NoRepeatArtist)) {
				artistList.erase(s++);
				//s = artistList.begin();
			} else {
				s++;
			}
		}

		ArtistListType::const_iterator x = artistList.find(Scan->meta->artist);
		if (x != artistList.end()) {
			int64 lp = tme - x->second;
			api->ib_printf2(pluginnum,_("AutoDJ -> Skipping Song (by Artist): %s (last played " I64FMT " secs ago, NoRepeatArtist = %u)\n"),Scan->fn,lp,ad_config.Options.NoRepeatArtist);
			return false;
		}
	}

	return true;
}

AD_CONFIG_PLAYLIST * GetPlaylist(const char * name, bool create_if_not_exists) {
	AutoMutex(QueueMutex);
	for (int i=0; i < ad_config.num_playlists; i++) {
		if (!stricmp(name, ad_config.Playlists[i].Name)) {
			return &ad_config.Playlists[i];
		}
	}

	if (create_if_not_exists) {
		sstrcpy(ad_config.Playlists[ad_config.num_playlists].Name, name);
		return &ad_config.Playlists[ad_config.num_playlists++];
	}

	return NULL;
}

int GetPlaylistNum(const char * name, bool create_if_not_exists) {
	AutoMutex(QueueMutex);

	for (int i=0; i < ad_config.num_playlists; i++) {
		if (!stricmp(name, ad_config.Playlists[i].Name)) {
			return i;
		}
	}

	if (create_if_not_exists) {
		sstrcpy(ad_config.Playlists[ad_config.num_playlists].Name, name);
		return ad_config.num_playlists++;
	}

	return -1;
}

typedef std::vector<string> lapLines;

int LAP_FindLine(lapLines * st, const char * line) {
	for (size_t i=0; i < st->size(); i++) {
		if (!stricmp(st->at(i).c_str(), line)) {
			return i;
		}
	}
	return -1;
}

void LoadAdvancedPlaylist(const char * fn) {
	AutoMutex(QueueMutex);
	FILE * fp = fopen(fn, "rb");
	if (fp == NULL) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Error opening '%s' for read: %s!\n"), fn, strerror(errno));
		return;
	}
	char buf[1024];
	lapLines lines;
	while (!feof(fp) && fgets(buf, sizeof(buf), fp)) {
		strtrim(buf);
		if (buf[0]) {
			lines.push_back(buf);
		}
	}
	fclose(fp);

	int n = LAP_FindLine(&lines, "[playlists]");
	if (n == -1) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Could not find [playlists] line in playlist!\n"));
		return;
	}
	for (size_t i=n+1; i < lines.size(); i++) {
		sstrcpy(buf, lines[i].c_str());
		if (buf[0] == '#' || buf[0] == '/') {
			continue;
		}
		if (buf[0] == '[') {
			break;
		}
		char * p = strchr(buf, '=');
		if (p) {
			*p = 0;
			p++;

			AD_CONFIG_PLAYLIST * pl = GetPlaylist(buf, true);
			pl->ContentDir[0] = 0;

			char * p3 = NULL;
			char * p2 = strtok_r(p, ";", &p3);
			while (p2) {
				if (pl->ContentDir[0]) {
					strlcat(pl->ContentDir, ";", sizeof(pl->ContentDir));
				}
				strlcat(pl->ContentDir, p2, sizeof(pl->ContentDir));
				if (pl->ContentDir[strlen(pl->ContentDir) - 1] != PATH_SEP) {
					strlcat(pl->ContentDir, PATH_SEPS, sizeof(pl->ContentDir));
				}
				p2 = strtok_r(NULL, ";", &p3);
			}
		}
	}

	n = LAP_FindLine(&lines, "[order]");
	if (n == -1) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Could not find [order] line in playlist!\n"));
		return;
	}
	for (size_t i=n+1; i < lines.size(); i++) {
		sstrcpy(buf, lines[i].c_str());
		if (buf[0] == '#' || buf[0] == '/') {
			continue;
		}
		if (buf[0] == '[') {
			break;
		}
		char * p = strchr(buf, '=');
		if (p) {
			*p = 0;
			p++;
		}
		int ind = GetPlaylistNum(buf, false);
		if (ind != -1) {
			if (p == NULL || !api->textfunc->get_range(p, &ad_config.PlaylistOrder[ad_config.num_playlist_orders].MinPlays, &ad_config.PlaylistOrder[ad_config.num_playlist_orders].MaxPlays)) {
				ad_config.PlaylistOrder[ad_config.num_playlist_orders].MinPlays = ad_config.PlaylistOrder[ad_config.num_playlist_orders].MaxPlays = 1;
			}
			ad_config.PlaylistOrder[ad_config.num_playlist_orders].Playlist = ind;
			ad_config.num_playlist_orders++;
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ -> Could not find playlist called '%s' (referenced in [order] section)!\n"), buf);
		}
	}
}

bool LoadConfig() {

	DS_CONFIG_SECTION * pSec = api->config->GetConfigSection(NULL, "AutoDJ");
	if (pSec == NULL) {
		api->ib_printf2(pluginnum,_("AutoDJ -> No 'AutoDJ' section found in your config!\n"));
		return false;
	}

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(pSec, "Server");
	if (sec == NULL) {
		api->ib_printf2(pluginnum,_("AutoDJ -> No 'AutoDJ/Server' section found in your config!\n"));
		return false;
	}
	char * p = NULL;
	//int j=0;

	api->config->GetConfigSectionValueBuf(sec, "password", ad_config.Server.Pass, sizeof(ad_config.Server.Pass));
	api->config->GetConfigSectionValueBuf(sec, "SourceIP", ad_config.Server.SourceIP, sizeof(ad_config.Server.SourceIP));

	SOUND_SERVER ss;
	api->ss->GetSSInfo(0, &ss);
	// Load mountpoint from SS by default
	sstrcpy(ad_config.Server.Mount, ss.mount);
	if (ad_config.Server.SourceIP[0] == 0) {
		sstrcpy(ad_config.Server.SourceIP, ss.host);
	}

	DRIFT_DIGITAL_SIGNATURE();
	if ((p = api->config->GetConfigSectionValue(sec,"description"))) {
		sstrcpy(ad_config.Server.Desc,p);
	}
	if ((p = api->config->GetConfigSectionValue(sec,"genre"))) {
		sstrcpy(ad_config.Server.Genre,p);
	}
	if ((p = api->config->GetConfigSectionValue(sec,"mount"))) {
		sstrcpy(ad_config.Server.Mount,p);
	}
	if ((p = api->config->GetConfigSectionValue(sec,"url"))) {
		sstrcpy(ad_config.Server.URL,p);
	}
	if ((p = api->config->GetConfigSectionValue(sec,"name"))) {
		sstrcpy(ad_config.Server.Name,p);
	}

	api->config->GetConfigSectionValueBuf(sec, "icq", ad_config.Server.ICQ, sizeof(ad_config.Server.ICQ)-1);

	if ((p = api->config->GetConfigSectionValue(sec,"aim"))) {
		sstrcpy(ad_config.Server.AIM,p);
	}
	if ((p = api->config->GetConfigSectionValue(sec,"irc"))) {
		sstrcpy(ad_config.Server.IRC,p);
	}

	if ((p = api->config->GetConfigSectionValue(sec,"MimeOverride"))) {
		sstrcpy(ad_config.Server.MimeType,p);
	}

	//bool using_advanced_playlist = false;
	if ((p = api->config->GetConfigSectionValue(sec,"Content"))) {
		char * tmp = zstrdup(p);
		char * ext = strrchr(tmp, '.');
		if (ext && !stricmp(ext, ".apl")) {
			//using_advanced_playlist = true;
			LoadAdvancedPlaylist(tmp);
		} else {
			AD_CONFIG_PLAYLIST * pl = GetPlaylist("main", true);
			pl->ContentDir[0] = 0;
			ad_config.PlaylistOrder[0].MaxPlays = ad_config.PlaylistOrder[0].MinPlays = 1;
			ad_config.PlaylistOrder[0].Playlist = 0;
			ad_config.num_playlist_orders = 1;
			char * p3 = NULL;
			char * p2 = strtok_r(tmp, ";", &p3);
	//		struct stat st;
			while (p2) {
				if (pl->ContentDir[0]) {
					strlcat(pl->ContentDir, ";", sizeof(pl->ContentDir));
				}
				strlcat(pl->ContentDir, p2, sizeof(pl->ContentDir));
				//if (stat(p2, &st) != 0 || S_ISDIR(st.st_mode)) {
				if (pl->ContentDir[strlen(pl->ContentDir) - 1] != PATH_SEP) {
					strlcat(pl->ContentDir, PATH_SEPS, sizeof(pl->ContentDir));
				}
				//}
				p2 = strtok_r(NULL, ";", &p3);
			}
		}
		zfree(tmp);
	}

	if ((p = api->config->GetConfigSectionValue(sec,"promo"))) {
		char * tmp = zstrdup(p);
		char * p3 = NULL;
		char * p2 = strtok_r(tmp, ";", &p3);
		while (p2) {
			if (ad_config.Server.PromoDir[0]) {
				strlcat(ad_config.Server.PromoDir, ";", sizeof(ad_config.Server.PromoDir));
			}
			strlcat(ad_config.Server.PromoDir, p2, sizeof(ad_config.Server.PromoDir));
			if (ad_config.Server.PromoDir[strlen(ad_config.Server.PromoDir) - 1] != PATH_SEP) {
				strlcat(ad_config.Server.PromoDir, PATH_SEPS, sizeof(ad_config.Server.PromoDir));
			}
			p2 = strtok_r(NULL, ";", &p3);
		}
		zfree(tmp);

		DRIFT_DIGITAL_SIGNATURE();
	}

	ad_config.Server.Public = api->config->GetConfigSectionLong(sec, "Public") > 0 ? true:false;
	ad_config.Server.Reset = api->config->GetConfigSectionLong(sec, "Reset") > 0 ? true:false;
	ad_config.Server.Bitrate = api->config->GetConfigSectionLong(sec, "Bitrate");
	if (ad_config.Server.Bitrate <= 0) { ad_config.Server.Bitrate = 64; }
	ad_config.Server.Channels = api->config->GetConfigSectionLong(sec, "Channels");
	if (ad_config.Server.Channels <= 0) { ad_config.Server.Channels = 1; }
	if (ad_config.Server.Channels > 2) { ad_config.Server.Channels = 2; }
	ad_config.Server.Sample = api->config->GetConfigSectionLong(sec, "Sample");
	if (ad_config.Server.Sample <= 0) { ad_config.Server.Sample = 44100; }
	ad_config.Server.SourcePort = api->config->GetConfigSectionLong(sec, "SourcePort");
	if (ad_config.Server.SourcePort < 0 || ad_config.Server.SourcePort > 65535) { ad_config.Server.SourcePort = 0; }

	p = api->config->GetConfigSectionValue(sec,"Encoder");
	if (p != NULL) {
		sstrcpy(ad_config.Server.EncoderName, p);
		if (!stricmp(ad_config.Server.EncoderName, "ogg")) {
			sstrcpy(ad_config.Server.EncoderName, "oggvorbis");
		}
	} else {
		sstrcpy(ad_config.Server.EncoderName, "mp3");
	}

	sec = api->config->GetConfigSection(pSec, "Options");
#ifdef WIN32
	strcpy(ad_config.Options.QueuePlugin, ".\\plugins\\AutoDJ\\adjq_memory.dll");
#else
	strcpy(ad_config.Options.QueuePlugin, "./plugins/AutoDJ/adjq_memory.so");
#endif
	ad_config.Options.Volume = 1;
	ad_config.Options.ID3_Mode = 1;
	ad_config.Options.EnableFind = true;
	ad_config.Options.EnableRequests = true;
	ad_config.Options.EnableTitleUpdates = true;
	ad_config.Options.EnableYP = true;

	if (sec == NULL) {
		api->ib_printf2(pluginnum,_("AutoDJ -> No 'AutoDJ/Options' section found in your config!\n"));
		return false;
	} else {
		if ((p = api->config->GetConfigSectionValue(sec,"MoveDir"))) {
			sstrcpy(ad_config.Options.MoveDir,p);
			if (ad_config.Options.MoveDir[strlen(ad_config.Options.MoveDir) - 1] == '\\') {
				ad_config.Options.MoveDir[strlen(ad_config.Options.MoveDir) - 1] = 0;
			}
			if (ad_config.Options.MoveDir[strlen(ad_config.Options.MoveDir) - 1] == '/') {
				ad_config.Options.MoveDir[strlen(ad_config.Options.MoveDir) - 1] = 0;
			}
		}
		if ((p = api->config->GetConfigSectionValue(sec, "YouTubeDir"))) {
			sstrcpy(ad_config.Options.YouTubeDir, p);
			if (ad_config.Options.YouTubeDir[strlen(ad_config.Options.YouTubeDir) - 1] == '\\') {
				ad_config.Options.YouTubeDir[strlen(ad_config.Options.YouTubeDir) - 1] = 0;
			}
			if (ad_config.Options.YouTubeDir[strlen(ad_config.Options.YouTubeDir) - 1] == '/') {
				ad_config.Options.YouTubeDir[strlen(ad_config.Options.YouTubeDir) - 1] = 0;
			}
		}

		if ((p = api->config->GetConfigSectionValue(sec, "VoiceTitle"))) {
			sstrcpy(ad_config.Voice.Title, p);
		} else {
			strcpy(ad_config.Voice.Title, "The Voice of AutoDJ");
		}

		if ((p = api->config->GetConfigSectionValue(sec, "VoiceArtist"))) {
			sstrcpy(ad_config.Voice.Artist, p);
		} else {
			strcpy(ad_config.Voice.Artist, "www.ShoutIRC.com");
		}

		if ((p = api->config->GetConfigSectionValue(sec,"NoPlayFilters"))) {
			sstrcpy(ad_config.Options.NoPlayFilters,p);
		}

		if ((p = api->config->GetConfigSectionValue(sec, "YouTubeExtra")) && strlen(p)) {
			sstrcpy(ad_config.Options.YouTubeExtra, p);
		}
		if ((p = api->config->GetConfigSectionValue(sec, "QueuePlugin")) && strlen(p)) {
			sstrcpy(ad_config.Options.QueuePlugin,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"RotationPlugin")) && strlen(p)) {
			sstrcpy(ad_config.Options.RotationPlugin,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"Resampler")) != NULL) {
			if (!stricmp(p, "libresample")) {
				sstrcpy(ad_config.Options.ResamplerPlugin, "plugins/AutoDJ/adjr_libresample." DL_SOEXT "");
			} else if (!stricmp(p, "soxr")) {
				sstrcpy(ad_config.Options.ResamplerPlugin, "plugins/AutoDJ/adjr_soxr." DL_SOEXT "");
			}
		}
		/*
		if ((p = api->config->GetConfigSectionValue(sec,"ResamplerPlugin")) && strlen(p) && stricmp(p, "speex")) {
			sstrcpy(ad_config.Options.ResamplerPlugin,p);
		}
		*/
		if ((p = api->config->GetConfigSectionValue(sec,"DSPPlugin")) && strlen(p)) {
			sstrcpy(ad_config.Options.DSPPlugin,p);
		}

		ad_config.Options.EnableFind = api->config->GetConfigSectionLong(sec, "EnableFind") != 0 ? true:false;
		ad_config.Options.EnableYP = api->config->GetConfigSectionLong(sec, "EnableYP") != 0 ? true:false;
		ad_config.Options.EnableTitleUpdates = api->config->GetConfigSectionLong(sec, "EnableTitleUpdates") != 0 ? true:false;
		ad_config.Options.StrictParse = api->config->GetConfigSectionLong(sec, "StrictParse") > 0 ? true:false;
		ad_config.Options.IncludeAlbum = api->config->GetConfigSectionLong(sec, "IncludeAlbum") > 0 ? true:false;
		ad_config.Options.EnableRequests = api->config->GetConfigSectionLong(sec, "EnableRequests") != 0 ? true:false;
		ad_config.Options.EnableCrossfade = api->config->GetConfigSectionLong(sec, "EnableCrossfade") != 0 ? true:false;
		ad_config.Options.AutoStart = api->config->GetConfigSectionLong(sec, "AutoStart") != 0 ? true:false;
		ad_config.Options.AutoPlayIfNoSource = api->config->GetConfigSectionLong(sec, "AutoPlayIfNoSource") > 0 ? true:false;
		ad_config.Options.OnlyScanNewFiles = api->config->GetConfigSectionLong(sec, "OnlyScanNewFiles") > 0 ? true:false;
		ad_config.Voice.EnableVoiceBroadcast = api->config->GetConfigSectionLong(sec, "EnableVoiceBroadcast") != 0 ? true:false;
		ad_config.Options.ScanDebug = api->config->GetConfigSectionLong(sec, "ScanDebug") > 0 ? true:false;
		ad_config.Options.ScanThreads = api->config->GetConfigSectionLong(sec, "ScanThreads");
		if (api->config->IsConfigSectionValue(sec, "Volume")) {
			ad_config.Options.Volume = api->config->GetConfigSectionFloat(sec, "Volume");
		}

		char buf[64];
		if (api->config->GetConfigSectionValueBuf(sec, "EnableVoice", buf, sizeof(buf))) {
			api->textfunc->get_range(buf, &ad_config.Voice.DoVoice, &ad_config.Voice.DoVoiceMax);
		} else {
			ad_config.Voice.DoVoice = ad_config.Voice.DoVoiceMax = 0;
		}
		ad_config.Voice.DoVoiceOnRequests = api->config->GetConfigSectionLong(sec, "DoVoiceOnRequests") > 0 ? true : false;
		if (api->config->GetConfigSectionValueBuf(sec, "DoPromos", buf, sizeof(buf))) {
			api->textfunc->get_range(buf, &ad_config.Options.DoPromos, &ad_config.Options.DoPromosMax);
			songs_until_promo = api->genrand_range(ad_config.Options.DoPromos, ad_config.Options.DoPromosMax);
		} else {
			ad_config.Options.DoPromos = ad_config.Options.DoPromosMax = songs_until_promo = 0;
		}
		ad_config.Options.NumPromos = api->config->GetConfigSectionLong(sec, "NumPromos") > 0 ? api->config->GetConfigSectionLong(sec, "NumPromos"):1;
		ad_config.Options.NoRepeat = api->config->GetConfigSectionLong(sec, "NoRepeat") > 0 ? api->config->GetConfigSectionLong(sec, "NoRepeat"):0;
		ad_config.Options.NoRepeatArtist = api->config->GetConfigSectionLong(sec, "NoRepeatArtist") > 0 ? api->config->GetConfigSectionLong(sec, "NoRepeatArtist"):0;
		ad_config.Options.MinReqTimePerSong = api->config->GetConfigSectionLong(sec, "MinReqTimePerSong") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerSong"):0;
		ad_config.Options.MinReqTimePerArtist = api->config->GetConfigSectionLong(sec, "MinReqTimePerArtist") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerArtist"):0;
		ad_config.Options.MinReqTimePerUser = api->config->GetConfigSectionLong(sec, "MinReqTimePerUser") > 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerUser"):0;
		ad_config.Options.MinReqTimePerUserMask = api->config->GetConfigSectionLong(sec, "MinReqTimePerUserMask") >= 0 ? api->config->GetConfigSectionLong(sec, "MinReqTimePerUserMask"):-1;
		ad_config.Options.DJConnectTime = api->config->GetConfigSectionLong(sec, "DJConnectTime") > 0 ? api->config->GetConfigSectionLong(sec, "DJConnectTime"):15;
		ad_config.Options.RequestDelay = api->config->GetConfigSectionLong(sec, "RequestDelay") > 0 ? api->config->GetConfigSectionLong(sec, "RequestDelay"):0;
		ad_config.Options.ID3_Mode = api->config->GetConfigSectionLong(sec, "ID3_Mode") >= 0 ? api->config->GetConfigSectionLong(sec, "ID3_Mode"):1;
		ad_config.Options.MaxSongDuration = api->config->GetConfigSectionLong(sec, "MaxSongDuration") >= 0 ? api->config->GetConfigSectionLong(sec, "MaxSongDuration"):0;
		ad_config.Options.AutoReload = api->config->GetConfigSectionLong(sec, "AutoReload") > 0 ? api->config->GetConfigSectionLong(sec, "AutoReload"):0;
		ad_config.Options.MaxRequests = api->config->GetConfigSectionLong(sec, "MaxRequests") > 0 ? api->config->GetConfigSectionLong(sec, "MaxRequests"):0;
		ad_config.Options.CrossfadeMinDuration = api->config->GetConfigSectionLong(sec, "CrossfadeMinDuration") > 30 ? api->config->GetConfigSectionLong(sec, "CrossfadeMinDuration")*1000:30000;
		ad_config.Options.CrossfadeLength = api->config->GetConfigSectionLong(sec, "CrossfadeLength") > 500 ? api->config->GetConfigSectionLong(sec, "CrossfadeLength"):6000;

		//if (ad_config.Options.EnableCrossfade && ad_config.Voice.DoVoice) {
			//api->ib_printf2(pluginnum,_("AutoDJ -> Both the AutoDJ Voice and Crossfader cannot be active at the same time, disabling crossfader...\n"));
			//ad_config.Options.EnableCrossfade = false;
		//}
	}

	sec = api->config->GetConfigSection(pSec, "FileLister");
	if (sec != NULL) {
		if ((p = api->config->GetConfigSectionValue(sec,"List"))) {
			sstrcpy(ad_config.FileLister.List,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"Header"))) {
			sstrcpy(ad_config.FileLister.Header,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"Footer"))) {
			sstrcpy(ad_config.FileLister.Footer,p);
		}
		if ((p = api->config->GetConfigSectionValue(sec,"NewChar"))) {
			sstrcpy(ad_config.FileLister.NewChar,p);
		}
		strcpy(ad_config.FileLister.Line,"%file%");
		if ((p = api->config->GetConfigSectionValue(sec,"Line"))) {
			sstrcpy(ad_config.FileLister.Line,p);
		}
	}

	char buf[128];
	char msgbuf[8192];
	for (int i=0; i < MAX_VOICE_MESSAGES; i++) {
		sprintf(buf, "ADJVoice_%d", i);
		if (api->LoadMessage(buf, msgbuf, sizeof(msgbuf))) {
			ad_config.Voice.Messages[ad_config.Voice.NumMessages] = zstrdup(msgbuf);
			ad_config.Voice.NumMessages++;
		}
	}

	return true;
}

bool LoadConfigPostProcess() {
	for (int i=0; i < MAX_ENCODERS; i++) {
		if (ad_config.Encoders[i] != NULL) {
			if (!stricmp(ad_config.Server.EncoderName, ad_config.Encoders[i]->name)) {
				ad_config.SelEncoder = ad_config.Encoders[i];
				ad_config.Encoder = ad_config.Encoders[i]->create();
				api->ib_printf2(pluginnum,_("AutoDJ -> Found encoder '%s' in slot %d!\n"), ad_config.Encoders[i]->name, i);
				break;
			}
		}
	}

	if (!ad_config.Encoder) {
		ad_config.SelEncoder = ad_config.Encoders[0];
		if (ad_config.Encoders[0]) {
			ad_config.Encoder = ad_config.Encoders[0]->create();
		} else {
			ad_config.Encoder = NULL;
		}
		if (ad_config.Encoder) {
			api->ib_printf2(pluginnum,_("AutoDJ -> Could not find your specified Encoder, defaulting to slot 0 (%s)\n"), ad_config.Encoders[0]->name);
		} else {
			api->ib_printf2(pluginnum,_("AutoDJ -> ERROR: You do not have any Encoders loaded!\n"));
			return false;
		}
	}

	if (ad_config.Encoder && !strlen(ad_config.Server.MimeType)) {
		sstrcpy(ad_config.Server.MimeType, ad_config.SelEncoder->mimetype);
	}

	return true;
}

QUEUE * AllocQueue() {
	QUEUE * ret = (QUEUE *)zmalloc(sizeof(QUEUE));
	memset(ret, 0, sizeof(QUEUE));
	return ret;
}

void FreeQueue(QUEUE * Scan) {
	zfreenn(Scan->fn);
	zfreenn(Scan->path);
	zfreenn(Scan->meta);
	zfreenn(Scan->isreq);
	DRIFT_DIGITAL_SIGNATURE();
	zfree(Scan);
}

QUEUE * CopyQueue(const QUEUE * Scan) {
	QUEUE * ret = AllocQueue();
	memcpy(ret, Scan, sizeof(QUEUE));
	ret->fn = zstrdup(Scan->fn);
	ret->path = zstrdup(Scan->path);
	if (Scan->isreq) {
		ret->isreq = (REQ_INFO *)zmalloc(sizeof(REQ_INFO));
		memcpy(ret->isreq,Scan->isreq,sizeof(REQ_INFO));
	}
	if (Scan->meta) {
		ret->meta = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
		memcpy(ret->meta,Scan->meta,sizeof(TITLE_DATA));
	}
	return ret;
}

READER_HANDLE * OpenFile(const char * fn, int32 mode) {
	if (!(mode & READER_RDWR)) {
		api->ib_printf("AutoDJ -> No file opening mode set for '%s'!\n", fn);
		return NULL;
	}
	for (int i=0; readers[i].is_my_file != NULL; i++) {
		if (readers[i].is_my_file(fn)) {
			READER_HANDLE * ret = readers[i].open(fn, mode);
			if (ret) {
				api->ib_printf2(pluginnum,_("AutoDJ -> Opened %s '%s' via %s\n"),ret->type,fn,ret->desc);
			}
			return ret;
		}
	}
	return NULL;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	dsl_init();
	memset(&ad_config,0,sizeof(ad_config));
	ad_config.pluginnum = pluginnum = num;
	ad_config.api = api = BotAPI;
	current_playlist = -1;
	GetAPI()->botapi = BotAPI;
	QueueMutex.SetLockTimeout(3600000);

	if (api->GetVersion() & IRCBOT_VERSION_FLAG_LITE) {
		api->ib_printf2(pluginnum,_("AutoDJ -> RadioBot Lite is not supported!\n"));
		return 0;
	}
	/*
	if (api->GetVersion() & IRCBOT_VERSION_FLAG_BASIC) {
		api->ib_printf2(pluginnum,_("AutoDJ -> RadioBot Basic is not supported!\n"));
		return 0;
	}
	*/
	if (api->ss == NULL) {// || !(api->GetVersion() & (IRCBOT_VERSION_FLAG_FULL|IRCBOT_VERSION_FLAG_STANDALONE))
		api->ib_printf2(pluginnum,_("AutoDJ -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	api->ib_printf2(pluginnum,_("AutoDJ -> Version %s, Platform: %s\n"), AUTODJ_VERSION, api->platform);

	if (!LoadConfig()) {
		return 0;
	}
	if (ad_config.Options.AutoStart) {
		ad_config.playing = true;
	}

	{
		//api->db->Query("DROP TABLE `AutoDJ_YouTube`;", NULL, NULL, NULL);
		api->db->Query("CREATE TABLE IF NOT EXISTS `AutoDJ_YouTube` (`URL` varchar(64) NOT NULL, `FN` varchar(255) NOT NULL, PRIMARY KEY  (`URL`));", NULL, NULL, NULL);
		//api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS `AutoDJ_YouTube_URL` ON `AutoDJ_YouTube`(`URL`);", NULL, NULL, NULL);
	}


#ifdef _WIN32
	LoadPlugins(".\\plugins\\");
#else
	LoadPlugins("./plugins/");
	if (ad_config.noplugins == 0){
		LoadPlugins("/usr/lib/shoutirc/plugins/");
	}
	if (ad_config.noplugins == 0){
		LoadPlugins("/usr/local/lib/shoutirc/plugins/");
	}
#endif

	if (!LoadConfigPostProcess()) {
		UnloadPlugins();
		return 0;
	}
	if (!LoadQueuePlugin(ad_config.Options.QueuePlugin)) {
		UnloadPlugins();
		return 0;
	}
	if (ad_config.Queue == NULL) {
		api->ib_printf2(pluginnum,_("AutoDJ -> ERROR: There is no Queue plugin loaded!\n"));
		UnloadPlugins();
		return 0;
	}
	if (strlen(ad_config.Options.RotationPlugin)) {
		LoadRotationPlugin(ad_config.Options.RotationPlugin);
	}
	if (strlen(ad_config.Options.ResamplerPlugin)) {
		LoadResamplerPlugin(ad_config.Options.ResamplerPlugin);
	}

	for (int i=0; i < NUM_DECKS; i++) {
		ad_config.Decks[i] = new Deck((AUTODJ_DECKS)i);
	}
	MixerStartup();

	//api->db->Query("CREATE TABLE IF NOT EXISTS autodj_ratings(FileID INTEGER PRIMARY KEY, Rating INTEGER DEFAULT 0, Votes INTEGER DEFAULT 0);", NULL, NULL, NULL);
	//api->db->Query("CREATE TABLE IF NOT EXISTS autodj_votes(FileID INTEGER, Rating INTEGER DEFAULT 0, Nick VARCHAR(255));", NULL, NULL, NULL);

	ad_config.sockets = new Titus_Sockets3;
	if (!ad_config.sockets->EnableSSL("ircbot.pem", TS3_SSL_METHOD_TLS)) {
		api->ib_printf2(pluginnum, _("AutoDJ -> Error enabling TLS support: %s!\n"), ad_config.sockets->GetLastErrorString());
	}
	DRIFT_DIGITAL_SIGNATURE();

	QueuePromoFiles(ad_config.Server.PromoDir);

	/*
	if (ad_config.Encoder == NULL) {
		ad_config.playing = false;
		api->ib_printf2(pluginnum,"AutoDJ -> ERROR: There is no Encoder plugin loaded!\n");
	}
	*/

	#if !defined(WIN32)
	api->ib_printf2(pluginnum,"AutoDJ -> Ignoring some signals...\n");
	signal(SIGSEGV,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	#endif

	if (ad_config.Voice.DoVoice && api->SendMessage(-1, TTS_GET_INTERFACE, (char *)&ad_config.tts, sizeof(&ad_config.tts)) != 1) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Error: TTS Services plugin is not loaded! You must load the TTS Services plugin in order to use the AutoDJ voice.\n"));
		ad_config.Voice.DoVoice = 0;
	}

#if defined(WIN32)
	ad_config.pTaskList = NULL;
	if (IsWindows7OrGreater()) {
		CoInitialize(NULL);
		//CLSCTX_INPROC_HANDLER
		HRESULT ret = CoCreateInstance(CLSID_TaskbarList, NULL,	CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (LPVOID*)&ad_config.pTaskList);
		if(SUCCEEDED(ret)) {
			ad_config.pTaskList->HrInit();
			ad_config.pTaskList->SetProgressState(api->hWnd, TBPF_NOPROGRESS);
		} else {
			ad_config.pTaskList = NULL;
			CoUninitialize();
		}
	}
#endif

	LoadState();

	TT_StartThread(PlayThread,     NULL, "Playback Control",	pluginnum);
	TT_StartThread(UpdateTitle,    NULL, "Title Updater",		pluginnum);
	TT_StartThread(ScheduleThread, NULL, "Scheduler",			pluginnum);

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-move",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This tells AutoDJ to move the currently playing file to a holding folder for review");
	api->commands->RegisterCommand_Proc(pluginnum, "move",				&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC						, AutoDJ_Commands, "This tells AutoDJ to move the currently playing file to a holding folder for review");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-relay",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This queues a file to play. Takes a pathname or stream://");
	api->commands->RegisterCommand_Proc(pluginnum, "relay",				&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC						, AutoDJ_Commands, "This queues a file to play. Takes a pathname or stream://");

	api->commands->RegisterCommand_Proc(pluginnum, "autodj-chroot",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE|CM_FLAG_ASYNC		, AutoDJ_Commands, "This tells AutoDJ to change it's song folder");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-speak",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This tells AutoDJ to speak the specified text at the end of the current song");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-modules",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This lets you see the plugins loaded in AutoDJ");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-encoders", &acl, CM_ALLOW_IRC_PRIVATE | CM_ALLOW_CONSOLE_PRIVATE, AutoDJ_Commands, "This lets you see the encoders loaded/supported in AutoDJ (for the Encoder line in AutoDJ/Server)");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-name", &acl, CM_ALLOW_IRC_PRIVATE | CM_ALLOW_CONSOLE_PRIVATE, AutoDJ_Commands, "This command will let you change the name AutoDJ uses on your sound server");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-volume",	    &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This command will let you change the volume of your audio (experimental)");

	api->commands->FillACL(&acl, UFLAG_ADVANCED_SOURCE, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-next",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This tells AutoDJ to skip the current song and move on to the next one");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-requests",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This command will toggle whether AutoDJ will take requests or not");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-songtitle",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This command will let you send an updated song title to your sound server");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-reqlist",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This will show you any files in the request queue");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-reqdelete",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This will delete a file from the request queue. You can use a filename or a wildcard pattern");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-reload",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE|CM_FLAG_ASYNC		, AutoDJ_Commands, "This tells AutoDJ to re-scan your folders for new songs and reload the schedule (if any)");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-force",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This command makes AutoDJ stop playing immediately");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-dopromo",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This command makes AutoDJ play a promo after the current song");
	api->commands->RegisterCommand_Proc(pluginnum, "youtube-dl", &acl, CM_ALLOW_IRC_PUBLIC | CM_ALLOW_IRC_PRIVATE | CM_ALLOW_CONSOLE_PRIVATE | CM_FLAG_ASYNC, AutoDJ_YT_Commands, "This command makes AutoDJ download a YouTube video.");
	api->commands->RegisterCommand_Proc(pluginnum, "youtube-play", &acl, CM_ALLOW_IRC_PUBLIC | CM_ALLOW_IRC_PRIVATE | CM_ALLOW_CONSOLE_PRIVATE | CM_FLAG_ASYNC, AutoDJ_YT_Commands, "This command makes AutoDJ download a YouTube video and queue it for playback.");

	api->commands->FillACL(&acl, UFLAG_BASIC_SOURCE, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-stop",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This command tells AutoDJ to count you in so you can DJ");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-play",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This command tells AutoDJ to start playing");

	api->commands->FillACL(&acl, 0, UFLAG_REQUEST, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "songby",			&acl, CM_ALLOW_ALL, AutoDJ_Commands, "This will let you request a random song by a certain artist");

	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "next",				&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC						, AutoDJ_Commands, "This tells AutoDJ to skip the current song and move on to the next one");
	api->commands->RegisterCommand_Proc(pluginnum, "autodj-countdown",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE						, AutoDJ_Commands, "This will countdown the current song to you until it is over");
	api->commands->RegisterCommand_Proc(pluginnum, "countdown",			&acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC						, AutoDJ_Commands, "This will countdown the current song to you until it is over");

	return 1;
}

void quit() {
	api->ib_printf2(pluginnum,_("AutoDJ -> Beginning shutdown...\n"));

	api->commands->UnregisterCommandByName( "relay" );
	api->commands->UnregisterCommandByName( "songby" );
	api->commands->UnregisterCommandByName( "move" );
	api->commands->UnregisterCommandByName( "countdown" );
	api->commands->UnregisterCommandByName( "autodj-countdown" );
	api->commands->UnregisterCommandByName( "next" );
	api->commands->UnregisterCommandByName( "autodj-relay" );
	api->commands->UnregisterCommandByName( "autodj-move" );
	api->commands->UnregisterCommandByName( "autodj-next" );
	api->commands->UnregisterCommandByName( "autodj-chroot" );
	api->commands->UnregisterCommandByName( "autodj-name" );
	api->commands->UnregisterCommandByName( "autodj-songtitle" );
	api->commands->UnregisterCommandByName( "autodj-speak" );
	api->commands->UnregisterCommandByName( "autodj-requests" );
	api->commands->UnregisterCommandByName( "autodj-reqlist" );
	api->commands->UnregisterCommandByName( "autodj-reqdelete" );
	api->commands->UnregisterCommandByName( "autodj-modules" );
	api->commands->UnregisterCommandByName( "autodj-reload" );
	api->commands->UnregisterCommandByName( "autodj-force" );
	api->commands->UnregisterCommandByName( "autodj-dopromo" );
	api->commands->UnregisterCommandByName( "autodj-stop" );
	api->commands->UnregisterCommandByName( "autodj-play" );
	api->commands->UnregisterCommandByName( "autodj-encoders" );

	ad_config.shutdown_now=true;
	ad_config.playagainat=0;
	ad_config.playing=false;
	ad_config.countdown=false;
	ad_config.countdown_now=false;
	ad_config.force_promo = false;

	int64 timeo = time(NULL) + 15;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf2(pluginnum,"AutoDJ -> Waiting on threads to die (%d) (Will wait " I64FMT " more seconds...)\n",TT_NumThreadsWithID(pluginnum),timeo - time(NULL));
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}

	LockMutex(QueueMutex);

	SaveState();

	api->ib_printf2(pluginnum,_("AutoDJ -> Freeing request queue...\n"));
	QUEUE * Scan=rQue;
	while (Scan != NULL) {
		QUEUE * toDel = Scan;
		Scan=Scan->Next;
		FreeQueue(toDel);
	}

	api->ib_printf2(pluginnum,_("AutoDJ -> Freeing promo queue...\n"));
	for (promoQueue::iterator x = promos.begin(); x != promos.end(); x++) {
		FreeQueue(*x);
	}

	api->ib_printf2(pluginnum,_("AutoDJ -> Freeing main file queue...\n"));
	Scan=pQue;
	while (Scan != NULL) {
		QUEUE * toDel = Scan;
		Scan=Scan->Next;
		FreeQueue(toDel);
	}

	if (ad_config.nextSong) {
		FreeQueue(ad_config.nextSong);
		ad_config.nextSong = NULL;
	}

	if (ad_config.Queue) { ad_config.Queue->FreePlayList(); }

	if (artistList.size()) {
		//api->ib_printf2(pluginnum,_("AutoDJ -> Freeing NoRepeatArtist artist list...\n"));
		artistList.clear();
	}
	if (requestList.size()) {
		requestList.clear();
	}
	if (requestArtistList.size()) {
		requestArtistList.clear();
	}
	if (requestUserList.size()) {
		requestUserList.clear();
	}

	RelMutex(QueueMutex);

	MixerShutdown();
	for (int i=0; i < NUM_DECKS; i++) {
		delete ad_config.Decks[i];
	}

	api->ib_printf2(pluginnum,_("AutoDJ -> Unloading plugins...\n"));
	UnloadResamplerPlugin();
	UnloadRotationPlugin();
	UnloadQueuePlugin();
	UnloadPlugins();

	api->ib_printf2(pluginnum,_("AutoDJ -> Freeing sockets...\n"));
	delete ad_config.sockets;
	ad_config.sockets = NULL;

	//api->ib_printf2(pluginnum,_("AutoDJ -> Freeing mutexes...\n"));

	int i=0;
	for (i=0; i < ad_config.Voice.NumMessages; i++) {
		if (ad_config.Voice.Messages[i]) {
			zfree(ad_config.Voice.Messages[i]);
		}
	}

#if defined(WIN32)
	if (ad_config.pTaskList) {
		ad_config.pTaskList->Release();
		ad_config.pTaskList = NULL;
		CoUninitialize();
	}
#endif

	api->ib_printf2(pluginnum,_("AutoDJ -> Plugin Shut Down!\n"));
	dsl_cleanup();
}

int AutoDJ_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[4096];//, buf2[4096];
	if (!stricmp(command, "autodj-encoders")) {
		ut->std_reply(ut, _("Loaded encoders:"));
		for (int i = 0; i < MAX_ENCODERS; i++) {
			if (ad_config.Encoders[i] != NULL) {
				snprintf(buf, sizeof(buf), _("%s (%s)"), ad_config.Encoders[i]->name, ad_config.Encoders[i]->mimetype);
				ut->std_reply(ut, buf);
			}
		}
		ut->std_reply(ut, _("End of list."));
		return 1;
	} else if (!stricmp(command, "autodj-move") || !stricmp(command, "move")) {
		if (strlen(ad_config.Options.MoveDir)) {
			if (parms && strlen(parms) && stricmp(parms, "now")) {
				LockMutex(QueueMutex);
				QUEUE * Scan = ad_config.Queue->FindFile(parms);
				RelMutex(QueueMutex);
				if (Scan != NULL) {
					string sfn = Scan->path;
					sfn += Scan->fn;
					if (MoveFromQueue(sfn.c_str())) {
						snprintf(buf, sizeof(buf), _("File '%s' moved out of main rotation..."), Scan->fn);
						ut->std_reply(ut, buf);
					} else {
						ut->std_reply(ut, _("Error moving the file, check the console or LogChan for details..."));
					}
					FreeQueue(Scan);
				} else {
					ut->std_reply(ut, _("I could not find that file!"));
				}
			} else if (ad_config.playing && ad_config.connected) {
				ut->std_reply(ut, _("I will attempt to move the file when playback is complete, watch the console or LogChan for results..."));
				ad_config.Decks[GetMixer()->curDeck]->SetMove();
				if (parms && !stricmp(parms, "now")) {
					api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
				}
			} else {
				ut->std_reply(ut, _("Unable to comply, I am not playing a song."));
			}
		} else {
			ut->std_reply(ut, _("Unable to comply, you did not specify a MoveDir in your AutoDJ section."));
		}
		return 1;
	}

	if (!stricmp(command,"autodj-modules")) {
		sprintf(buf,_("Number of AutoDJ Plugins: %d"),ad_config.noplugins);
		ut->std_reply(ut, buf);
		for (int i=0; i < ad_config.noplugins; i++) {
			sprintf(buf,_("[%u] [%p] [%02d] [Name: %s]"),i,ad_config.Plugins[i].plug,ad_config.Plugins[i].plug->version,ad_config.Plugins[i].plug->plugin_desc);
			ut->std_reply(ut, buf);
		}
		ut->std_reply(ut, _("End of AutoDJ Plugins"));

		return 1;//stricmp(command,"modules") ? 1:0; //don't interfere with RadioBot's internal modules
	}

	DRIFT_DIGITAL_SIGNATURE();

	if (!stricmp(command, "autodj-reload")) {
		ut->std_reply(ut, _("Reloading schedule..."));
		LoadSchedule();
		ut->std_reply(ut, _("Beginning re-queue..."));
		QueueContentFiles(true);
		sprintf(buf, _("Re-queue complete! (Songs: %d)"), ad_config.num_files);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "autodj-stop")) {
		//ut->std_reply(ut, "Stopping after current song...");
		RefUser(ut);
		LockMutex(RepMutex);
		if (ad_config.repto) {
			UnrefUser(ad_config.repto);
		}
		ad_config.repto = ut;
		RelMutex(RepMutex);
		ad_config.countdown = true;
		if (parms && !stricmp(parms, "now") && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
			//ad_config.stopAt = time(NULL)+10;
			ad_config.countdown_now=true;
		} else {
			ut->std_reply(ut, _("Stopping after current song..."));
		}
		return 1;
	}

	DRIFT_DIGITAL_SIGNATURE();

	if (!stricmp(command, "autodj-force")) {
		ut->std_reply(ut, _("Stopping immediately..."));
		api->SendMessage(pluginnum, SOURCE_C_STOP, NULL, 0);
		return 1;
	}

	if (!stricmp(command, "autodj-play")) {
		ut->std_reply(ut, _("Playing..."));
		api->SendMessage(pluginnum, SOURCE_C_PLAY, NULL, 0);
		return 1;
	}

	if (!stricmp(command, "autodj-chroot") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: autodj-chroot [playlistname] dir"));
		return 1;
	}

	if (!stricmp(command,"autodj-chroot")) {
		StrTokenizer st((char *)parms, ' ');
		string pl = st.stdGetSingleTok(1);
		AD_CONFIG_PLAYLIST * p = GetPlaylist(pl.c_str(), false);
		int first = 2;
		if (p == NULL) {
			first = 1;
			p = GetPlaylist("main", false);
		}
		if (p) {
			sstrcpy(p->ContentDir, st.stdGetTok(first,st.NumTok()).c_str());
			if (p->ContentDir[strlen(p->ContentDir) - 1] != PATH_SEP) {
				strlcat(p->ContentDir, PATH_SEPS, sizeof(p->ContentDir));
			}
			snprintf(buf,sizeof(buf),_("Changed content dir for playlist '%s' to %s"), p->Name, p->ContentDir);
			ut->std_reply(ut, buf);
			AutoDJ_Commands("autodj-reload", NULL, ut, type);
		} else {
			snprintf(buf,sizeof(buf),_("Could not find playlist '%s'!"), pl.c_str());
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	if (!stricmp(command, "autodj-name") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: autodj-name NewName"));
		return 1;
	}

	if (!stricmp(command,"autodj-name")) {
		strlcpy(ad_config.Server.Name, parms, sizeof(ad_config.Server.Name));
		sprintf(buf,_("Changed AutoDJ name to %s. (Note: the name will not change until the AutoDJ reconnects to the sound server)"),ad_config.Server.Name);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "autodj-songtitle") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: autodj-songtitle Song title to send"));
		return 1;
	}

	if (!stricmp(command,"autodj-songtitle")) {

		TITLE_DATA td;
		memset(&td, 0, sizeof(TITLE_DATA));
		strlcpy(td.title, parms, sizeof(td.title));
		QueueTitleUpdate(&td);
		sstrcpy(buf,_("Queued title update...."));
		ut->std_reply(ut, buf);
		return 1;
	}

	DRIFT_DIGITAL_SIGNATURE();

	if (!stricmp(command, "autodj-speak") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: autodj-speak text"));
		return 1;
	}

	if (!stricmp(command,"autodj-speak")) {
		if (ad_config.Voice.DoVoice) {
			sstrcpy(ad_config.voice_override,parms);
			ut->std_reply(ut, _("I will speak at the end of this song..."));
		} else {
			ut->std_reply(ut, _("The AutoDJ voice is not enabled!"));
		}
		return 1;
	}

	if ((!stricmp(command,"autodj-next") || !stricmp(command,"next")) && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		if (ad_config.playing && ad_config.connected) {
			ut->std_reply(ut, _("Skipping song..."));
			api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
		} else {
			ut->std_reply(ut, _("Error: I am not playing a song right now!"));
		}
		return 1;
	}

	if (!stricmp(command, "autodj-volume") && (!parms || !strlen(parms))) {
		if (!ad_config.playing || !ad_config.connected) {
			return 0;
		}
		ut->std_reply(ut, _("Usage: !autodj-volume x (0.0 = silence, 1.0 = default volume)"));
		return 1;
	}

	if (!stricmp(command,"autodj-volume")) {
		if (!ad_config.playing || !ad_config.connected) {
			return 0;
		}
		ad_config.Options.Volume = atof(parms);
		sprintf(buf, _("New volume: %.3f"), ad_config.Options.Volume);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"autodj-reqlist")) {
		if (!ad_config.playing || !ad_config.connected) {
			return 0;
		}
		int num = 0;
		ut->std_reply(ut, _("Listing current requests..."));
		LockMutex(QueueMutex);
		QUEUE * Scan= rQue;
		while (Scan) {
			sprintf(buf, _("[%08X] %s"), Scan, Scan->fn);
			ut->std_reply(ut, buf);
			num++;
			Scan = Scan->Next;
		}
		sprintf(buf, _("Number of requests: %d"), num);
		ut->std_reply(ut, buf);
		ut->std_reply(ut, _("Use !autodj-reqdelete filename to remove the first request that matches that filename (wildcards supported)"));
		RelMutex(QueueMutex);
		return 1;
	}

	if (!stricmp(command, "autodj-reqdelete") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: autodj-reqdelete filename"));
		return 1;
	}

	if (!stricmp(command,"autodj-reqdelete")) {
		if (!ad_config.playing || !ad_config.connected) {
			return 0;
		}
		LockMutex(QueueMutex);
		QUEUE * Scan= rQue;
		bool done = false;
		while (Scan) {
			if (wildicmp(parms, Scan->fn)) {
				if (ad_config.Queue && ad_config.Queue->on_song_request_delete) {
					ad_config.Queue->on_song_request_delete(Scan);
				}
				sprintf(buf, _("Removed request %08X"), Scan);
				ut->std_reply(ut, buf);
				if (Scan == rQue) {
					rQue = rQue->Next;
					if (rQue) {
						rQue->Prev = NULL;
					}
				} else if (Scan->Next == NULL) {
					Scan->Prev->Next = NULL;
				} else {
					Scan->Prev->Next = Scan->Next;
					Scan->Next->Prev = Scan->Prev;
				}
				FreeQueue(Scan);
				done = true;
				break;
			}
			Scan = Scan->Next;
		}
		if (!done) {
			ut->std_reply(ut, _("Error: No request matching that filename or wildcard found!"));
		}
		RelMutex(QueueMutex);
		return 1;
	}

	if (!stricmp(command,"autodj-requests")) {
		if (parms && strlen(parms)) {
			if (!stricmp(parms, "1") || !stricmp(parms, "on")) {
				ad_config.Options.EnableRequests = true;
			} else {
				ad_config.Options.EnableRequests = false;
			}
		} else {
			ad_config.Options.EnableRequests = ad_config.Options.EnableRequests ? false:true;
		}
		if (ad_config.playing && ad_config.connected) {
			if (ad_config.Options.EnableRequests) {
				api->ss->EnableRequestsHooked(&adj_req_iface);
			} else {
				api->ss->EnableRequests(false);
			}
		}
		sprintf(buf, _("Requests are now: %s"), ad_config.Options.EnableRequests ? _("Enabled"):_("Disabled"));
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "autodj-dopromo")) {
		if (ad_config.playing && ad_config.connected) {
			ad_config.force_promo = true;
			sstrcpy(buf, _("I will play a promo next..."));
		} else {
			sstrcpy(buf, _("I am not the current DJ!"));
		}
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"autodj-lock")) {
		LockMutex(QueueMutex);
		if (ad_config.lock_user) {
			UnrefUser(ad_config.lock_user);
		}
		ad_config.lock_user = ut->User;
		RefUser(ut->User);
		RelMutex(QueueMutex);
		snprintf(buf, sizeof(buf), _("I will only accept requests from you until I you tell me %cautodj-release"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"autodj-release")) {
		LockMutex(QueueMutex);
		if (ad_config.lock_user) {
			UnrefUser(ad_config.lock_user);
			ad_config.lock_user = NULL;
			ut->std_reply(ut, _("I will now accept requests from everyone again."));
		} else {
			ut->std_reply(ut, _("AutoDJ is not currently locked!"));
		}
		RelMutex(QueueMutex);
		return 1;
	}

	if (!stricmp(command, "songby") && (!parms || !strlen(parms))) {
		if (!ad_config.playing || !ad_config.connected || !ad_config.Options.EnableRequests || !ad_config.Options.EnableFind) {
			return 0;
		}
		ut->std_reply(ut, _("Usage: !songby artist"));
		return 1;
	}

	if (!stricmp(command,"songby")) {
		if (!ad_config.playing || !ad_config.connected || !ad_config.Options.EnableRequests || !ad_config.Options.EnableFind) {
			return 0;
		}

		if (strchr(parms,'/') || strchr(parms,'\\')) {
			ut->std_reply(ut, _("Sorry, I could not find that file in my playlist!"));
			return 1;
		}

		LockMutex(QueueMutex);

		QUEUE_FIND_RESULT * q = ad_config.Queue->FindByMeta(parms, META_ARTIST);
		QUEUE * Found = NULL;
		if (q && q->num) {
			int tries=0;
			while (!Found && tries++ < 30) {
				Found = q->results[AutoDJ_Rand()%q->num];
			}
		}
		if (!Found) {
			if (q) { ad_config.Queue->FreeFindResult(q); }
			ut->std_reply(ut, _("Sorry, I could not find that file in my playlist!"));
			RelMutex(QueueMutex);
			return 1;
		}

		Found = CopyQueue(Found);
		if (q) { ad_config.Queue->FreeFindResult(q); }

		QUEUE * Scan=NULL;
		if (!(ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
			Scan=rQue;
			while(Scan != NULL) {
				if (Scan->ID == Found->ID) {
					ut->std_reply(ut, _("That file has already been requested, it should play soon..."));
					FreeQueue(Found);
					RelMutex(QueueMutex);
					return 1;
				}
				Scan = Scan->Next;
			}
		}

		Scan = Found;
		REQ_INFO *req = (REQ_INFO *)zmalloc(sizeof(REQ_INFO));
		memset(req, 0, sizeof(REQ_INFO));
		if (!stricmp(ut->Desc, "IRC")) {
			req->netno=ut->NetNo;
		} else {
			req->netno=-1;
		}
		sstrcpy(req->nick,ut->Nick);
		if (ut->Channel) {
			sstrcpy(req->channel,ut->Channel);
		}
		if (ad_config.Options.RequestDelay) {
			req->playAfter = time(NULL) + ad_config.Options.RequestDelay;
		}
		Scan->isreq = req;
		Scan->Next = NULL;

		//QUEUE *Scan2 = rQue;
		//int num=0;
		if (api->user->uflag_have_any_of(ut->Flags, UFLAG_MASTER|UFLAG_OP) || AllowRequest(Scan, ut)) {
			int num = AddRequest(Scan, ut->User ? ut->User->Nick:ut->Nick);
			sprintf(buf,_("Thank you for your request. I have selected %s by %s. It will play after %d more song(s)..."),Scan->meta->title,Scan->meta->artist,num);
		} else {
			sstrcpy(buf,_("Sorry, that song has already been requested recently..."));
			FreeQueue(Scan);
		}
		ut->std_reply(ut, buf);

		RelMutex(QueueMutex);
		return 1;
	}

	if ((!stricmp(command, "relay") || !stricmp(command, "autodj-relay")) && (!parms || !strlen(parms))) {
		if (!ad_config.playing || !ad_config.connected) {
			return 0;
		}
		ut->std_reply(ut, _("Usage: relay /path/to/file"));
		return 1;
	}

	if (!stricmp(command,"relay") || !stricmp(command,"autodj-relay")) {
		if (!ad_config.playing || !ad_config.connected) {
			return 0;
		}
		QUEUE * ret = AllocQueue();
		ret->fn = zstrdup(parms);
		ret->path = zstrdup("");

		/*
		TITLE_DATA * td = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
		if (ReadMetaData(buf2, td) == 1) {
			ret->meta = td;
		} else {
			zfree(td);
		}
		*/

		LockMutex(QueueMutex);
		AddRequest(ret, ut->User ? ut->User->Nick:ut->Nick, true);
		RelMutex(QueueMutex);

		sprintf(buf, _("Set to relay '%s' after song ends..."),parms);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"countdown") || !stricmp(command,"autodj-countdown")) {
		if (!ad_config.playing || !ad_config.connected) {
			return 0;
		}
		RefUser(ut);
		LockMutex(RepMutex);
		countdowns.push_back(ut);
		RelMutex(RepMutex);
		ut->std_reply(ut, _("I will count down the current song for you..."));
		return 1;
	}

	return 0;
}

bool LoadSongInfo(IBM_SONGINFO * si) {
				AutoMutex(QueueMutex);
				memset(si, 0, sizeof(IBM_SONGINFO));
				if (ad_config.playing && ad_config.connected && ad_config.cursong && ad_config.Decks[GetMixer()->curDeck]->isPlaying()) {
					snprintf(si->fn, sizeof(si->fn), "%s%s", ad_config.cursong->path, ad_config.cursong->fn);
					si->songLen = ad_config.cursong->songlen;
					si->file_id = ad_config.cursong->ID;
					if (ad_config.cursong->meta) {
						strlcpy(si->title, ad_config.cursong->meta->title, sizeof(si->title));
						strlcpy(si->artist, ad_config.cursong->meta->artist, sizeof(si->artist));
						strlcpy(si->album, ad_config.cursong->meta->album, sizeof(si->album));
						strlcpy(si->genre, ad_config.cursong->meta->genre, sizeof(si->genre));
					}
					si->is_request = ad_config.cursong->isreq ? true:false;
					if (ad_config.cursong->isreq && ad_config.cursong->isreq->nick[0]) {
						sstrcpy(si->requested_by, ad_config.cursong->isreq->nick);
					}
					si->playback_length = ad_config.Decks[GetMixer()->curDeck]->getLength();
					si->playback_position = ad_config.Decks[GetMixer()->curDeck]->getOutputTime();
					return true;
				}
				return false;
}

int remote(USER_PRESENCE * ut, unsigned char cliversion, REMOTE_HEADER * head, char * data) {
	char buf[1024];
	if (head->ccmd == RCMD_SRC_COUNTDOWN && (ut->Flags & UFLAG_BASIC_SOURCE)) {
		if (ad_config.playing && ad_config.connected) {
			sstrcpy(buf,_("Stopping after current song..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);

			RefUser(ut);
			LockMutex(RepMutex);
			if (ad_config.repto) {
				UnrefUser(ad_config.repto);
			}
			ad_config.repto = ut;
			RelMutex(RepMutex);
			ad_config.countdown=true;
		} else {
			sstrcpy(buf,_("AutoDJ is not currently playing..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);
		}
		return 1;
	}

	if (head->ccmd == RCMD_SRC_FORCE_OFF && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		if (ad_config.playing) {
			sstrcpy(buf,_("Stopping immediately..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);
			api->SendMessage(pluginnum, SOURCE_C_STOP, NULL, 0);
		} else {
			sstrcpy(buf,_("AutoDJ is not currently playing..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);
		}
		return 1;
	}

	if (head->ccmd == RCMD_SRC_FORCE_ON && (ut->Flags & UFLAG_BASIC_SOURCE)) {
		sstrcpy(buf,_("Playing..."));
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		api->SendMessage(pluginnum, SOURCE_C_PLAY, NULL, 0);
		ad_config.playing=true;
		return 1;
	}

	if (head->ccmd == RCMD_SRC_NEXT && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		if (ad_config.playing && ad_config.connected) {
			sstrcpy(buf,_("Skipping song..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);
			api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
			return 1;
		}
	}

	if (head->ccmd == RCMD_SRC_RELOAD && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		sstrcpy(buf,_("Reloading AutoDJ..."));
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);

		LoadSchedule();
		QueueContentFiles(true);

		//api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_STATUS) {
		if (ad_config.playing && ad_config.connected) {
			sstrcpy(buf, "playing");
		} else if (ad_config.playing) {
			sstrcpy(buf, "connecting");
		} else {
			sstrcpy(buf, "stopped");
		}
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_GET_NAME) {
		if (ad_config.playing && ad_config.connected) {
			sstrcpy(buf, "autodj");
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);
			return 1;
		}
	}

	if (head->ccmd == RCMD_SRC_GET_SONG && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		LockMutex(QueueMutex);
		if (ad_config.connected && ad_config.playing && ad_config.cursong) {
			head->scmd = RCMD_GENERIC_MSG;
			strlcpy(buf, ad_config.cursong->path, sizeof(buf));
			strlcat(buf, ad_config.cursong->fn, sizeof(buf));
		} else {
			head->scmd = RCMD_GENERIC_ERROR;
			sstrcpy(buf,_("Error: I'm not currently playing any song"));
		}
		RelMutex(QueueMutex);
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_GET_SONG_INFO && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		IBM_SONGINFO si;
		if (LoadSongInfo(&si)) {
			head->scmd = RCMD_SONG_INFO;
			head->datalen = sizeof(IBM_SONGINFO);
			api->SendRemoteReply(ut->Sock,head,(const char *)&si);
			return 1;
		}
		return 0;
	}

	if (head->ccmd == RCMD_SRC_RELAY && (ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
		if (!ad_config.playing || !ad_config.connected || data == NULL || data[0] == 0) {
			return 0;
		}
		QUEUE * ret = AllocQueue();
		ret->fn = zstrdup(data);
		ret->path = zstrdup("");

		LockMutex(QueueMutex);
		AddRequest(ret, ut->User ? ut->User->Nick:ut->Nick, true);
		RelMutex(QueueMutex);

		sprintf(buf, _("Set to relay '%s' after song ends..."),data);
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_RATE_SONG && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		head->scmd = RCMD_GENERIC_ERROR;
		if (api->ss->AreRatingsEnabled()) {
			char * p2 = NULL;
			char * nick = strtok_r(data, "\xFE", &p2);
			if (nick) {
				char * srating = strtok_r(NULL, "\xFE", &p2);
				if (srating) {
					int32 rating = atoi(srating);
					if (rating > 0 && rating <= api->ss->GetMaxRating()) {
						char * fn = strtok_r(NULL, "\xFE", &p2);
						if (fn) {
							LockMutex(QueueMutex);
							QUEUE * song = ad_config.Queue->FindFile(fn);
							if (song) {
								api->ss->RateSongByID(song->ID, nick, rating);
								strcpy(buf, "Thank you for your song rating.");
								head->scmd = RCMD_GENERIC_MSG;
								FreeQueue(song);
							} else {
								strcpy(buf, _("Error: I could not find that file in my playlist!"));
							}
							RelMutex(QueueMutex);
						} else {
							strcpy(buf, "Error: Data should be nick\\xFErating\\xFEfilename");
						}
					} else {
						snprintf(buf, sizeof(buf), "Error: Rating should be a string representing 0-%d", api->ss->GetMaxRating());
					}
				} else {
					strcpy(buf, "Error: Data should be nick\\xFErating\\xFEfilename");
				}
			} else {
				strcpy(buf, "Error: Data should be nick\\xFErating\\xFEfilename");
			}
		} else {
			strcpy(buf, "Error: Song Ratings are not enabled in RadioBot!");
		}
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
	}

	return 0;
}

bool ShouldIStop() {
	if (ad_config.shutdown_now) { return true; }
	if (ad_config.playing == false) { return true; }
	return false;
}

bool AllowRequest(QUEUE * Scan, USER_PRESENCE * ut) {
	if (QueueMutex.LockingThread() != GetCurrentThreadId()) {
		api->ib_printf2(pluginnum,"AutoDJ -> QueueMutex should be locked when calling AllowRequest!\n");
	}
	AutoMutex(QueueMutex);

	if (ad_config.lock_user && ut->User != ad_config.lock_user) {
		return false;
	}

	if (ad_config.Options.MinReqTimePerSong != 0) {
		RequestListType::iterator s = requestList.begin();
		while (s != requestList.end()) {
			time_t tmp = s->second;
			if (tmp <= (time(NULL)-ad_config.Options.MinReqTimePerSong)) {
				requestList.erase(s);
				s = requestList.begin();
			} else {
				s++;
			}
		}

		RequestListType::const_iterator x = requestList.find(Scan->ID);
		if (x != requestList.end()) {
			return false;
		}
	}

	if (Scan->meta && Scan->meta->artist[0] && ad_config.Options.MinReqTimePerArtist != 0) {
		RequestUserListType::iterator s = requestArtistList.begin();
		while (s != requestArtistList.end()) {
			time_t tmp = s->second;
			if (tmp <= (time(NULL)-ad_config.Options.MinReqTimePerArtist)) {
				requestArtistList.erase(s);
				s = requestArtistList.begin();
			} else {
				s++;
			}
		}

		RequestUserListType::const_iterator x = requestArtistList.find(Scan->meta->artist);
		if (x != requestArtistList.end()) {
			return false;
		}
	}

	//const char * nick = ut->User ? ut->User->Nick:ut->Nick;
	char mask[1024];
	if (ad_config.Options.MinReqTimePerUserMask < 0 || !api->user->Mask(ut->Hostmask, mask, sizeof(mask), ad_config.Options.MinReqTimePerUserMask)) {
		sstrcpy(mask, ut->User ? ut->User->Nick:ut->Nick);
	}
	if (ad_config.Options.MinReqTimePerUser > 0 && mask[0]) {
		RequestUserListType::iterator s = requestUserList.begin();
		time_t timeout =  time(NULL) - ad_config.Options.MinReqTimePerUser;
		while (s != requestUserList.end()) {
			time_t tmp = s->second;
			if (tmp <= timeout) {
				requestUserList.erase(s);
				s = requestUserList.begin();
			} else {
				s++;
			}
		}

		RequestUserListType::const_iterator x = requestUserList.find(mask);
		if (x != requestUserList.end()) {
			return false;
		}
	}

	return true;
}

int AddRequest(QUEUE * Scan, const char * nick, bool to_front) {
	if (QueueMutex.LockingThread() != GetCurrentThreadId()) {
		api->ib_printf2(pluginnum,"AutoDJ -> QueueMutex should be locked when calling AddRequest!\n");
	}
	AutoMutex(QueueMutex);

	int ret=0;
	Scan->Next = NULL;
	if (rQue == NULL) {
		rQue = Scan;
		Scan->Prev=NULL;
	} else {
		ret++;
		if (to_front) {
			Scan->Next = rQue;
			rQue->Prev = Scan;
			rQue = Scan;
			Scan->Prev = NULL;
		} else {
			QUEUE *Scan2 = rQue;
			while (Scan2->Next != NULL) {
				ret++;
				Scan2=Scan2->Next;
			}
			Scan2->Next=Scan;
			Scan->Prev = Scan2;
		}
	}

	if (ad_config.Options.MinReqTimePerSong != 0) {
		requestList[Scan->ID] = time(NULL);
	}
	if (ad_config.Options.MinReqTimePerArtist != 0 && Scan->meta && Scan->meta->artist[0]) {
		requestArtistList[Scan->meta->artist] = time(NULL);
	}
	if (ad_config.Options.MinReqTimePerUser != 0 && nick) {
		requestUserList[nick] = time(NULL);
	}

	if (ad_config.Queue->on_song_request) { ad_config.Queue->on_song_request(Scan, nick); }
	return ret;
}

QUEUE * MakeVoiceMP3(QUEUE * que) {
	QUEUE * ret = NULL;

	char * msgbuf = (char *)zmalloc(8192);
	char * msgbuf2 = (char *)zmalloc(8192);

	//TITLE_DATA td;
	//memset(&td, 0, sizeof(td));

	if (que->meta && strlen(que->meta->title)) {
		if (strlen(que->meta->artist)) {
			snprintf(msgbuf2, 8192, "%s by %s", que->meta->title, que->meta->artist);
		} else {
			strlcpy(msgbuf2, que->meta->title, 8192);
		}
	} else {
		strlcpy(msgbuf2, que->fn, 8192);
		char *ep = strrchr(msgbuf2, '.');
		if (ep) { ep[0] = 0; }
	}

	str_replace(msgbuf2, 8192, "_", " ");
	str_replace(msgbuf2, 8192, "-", ".");
	str_replace(msgbuf2, 8192, "[", "");
	str_replace(msgbuf2, 8192, "]", "");
	str_replace(msgbuf2, 8192, "(", "");
	str_replace(msgbuf2, 8192, ")", "");
	str_replace(msgbuf2, 8192, "feat.", "featuring");
	str_replace(msgbuf2, 8192, "Feat.", "featuring");
	str_replace(msgbuf2, 8192, " ft.", " featuring");
	str_replace(msgbuf2, 8192, " Ft.", " featuring");
	str_replace(msgbuf2, 8192, " pt.", " part");
	str_replace(msgbuf2, 8192, " Pt.", " part");
	while (strstr(msgbuf2, "  ")) {
		str_replace(msgbuf2, 8192, "  ", " ");
	}

	if (ad_config.Voice.NumMessages > 0) {
		int n = AutoDJ_Rand()%ad_config.Voice.NumMessages;
		if (n < 0) { n = 0; }
		if (n >= ad_config.Voice.NumMessages) { n = ad_config.Voice.NumMessages - 1; }
		strcpy(msgbuf, ad_config.Voice.Messages[n]);
		str_replace(msgbuf, 8192, "%song", msgbuf2);
	} else {
		switch(AutoDJ_Rand()%7) {
			case 0:
				sprintf(msgbuf, "You are listening to Shout I R C Radio. Coming up next: %s", msgbuf2);
				break;
			case 1:
				sprintf(msgbuf, "This is Auto DJ, the bot with Style. Coming up next: %s", msgbuf2);
				break;
			case 2:
				sprintf(msgbuf, "This is Auto DJ spinning. Next up is: %s", msgbuf2);
				break;
			case 3:
				sprintf(msgbuf, "Coming up next: %s", msgbuf2);
				break;
			case 4:
				sprintf(msgbuf, "You are listening to Shout I R C Radio. Next up is: %s", msgbuf2);
				break;
			case 5:
				sprintf(msgbuf, "This is Auto DJ spinning. Coming up next: %s", msgbuf2);
				break;
			default:
				sprintf(msgbuf, "Now playing: %s", msgbuf2);
				break;
		}
	}

	api->ProcText(msgbuf, 8192);

	if (api->irc && ad_config.Voice.EnableVoiceBroadcast) {
		char * msgbuf3 = (char *)zstrdup(msgbuf);
		str_replace(msgbuf3, strlen(msgbuf3)+1, "Shout I R C", "ShoutIRC");
		str_replace(msgbuf3, strlen(msgbuf3)+1, " I R C.", "IRC.");
		str_replace(msgbuf3, strlen(msgbuf3)+1, " I R C ", "IRC");
		api->BroadcastMsg(NULL, msgbuf3);
		zfree(msgbuf3);
	}

	if (que->isreq && que->isreq->nick[0]) {
		if ((strlen(msgbuf)+strlen(_("\nThis song was requested by "))+strlen(que->isreq->nick)) < 8192) {
			strlcat(msgbuf, _("\nThis song was requested by "), 8192);
			strlcat(msgbuf, que->isreq->nick, 8192);
		}
	}

	if (strlen(ad_config.voice_override)) {
		strlcpy(msgbuf,ad_config.voice_override, 8192);
		ad_config.voice_override[0] = 0;
	}

	if (api->irc) { api->irc->LogToChan(msgbuf); }

	char * txtfn = zmprintf("%sautodj_script.txt", api->GetBasePath());
	FILE * fp = fopen(txtfn, "wb");
	zfree(txtfn);
	if (fp) {
		fwrite(msgbuf,strlen(msgbuf),1,fp);
		fclose(fp);

		TITLE_DATA vd;
		memset(&vd, 0, sizeof(vd));
		sstrcpy(vd.title,  ad_config.Voice.Title);
		sstrcpy(vd.artist, ad_config.Voice.Artist);

		TTS_JOB job;
		memset(&job, 0, sizeof(job));
		job.inputFN = zmprintf("%sautodj_script.txt", api->GetBasePath());
		job.channels = ad_config.Server.Channels <= 2 ? ad_config.Server.Channels : 2;
		job.sample = ad_config.Server.Sample;
		if (ad_config.tts.GetPreferredOutputFormat(VE_DEFAULT) == VOF_WAV && GetFileDecoder("autodj_script.wav") != NULL) {
			job.outputFN = zmprintf("%sautodj_script.wav", api->GetBasePath());
			if (access(job.outputFN, 0) == 0) { remove(job.outputFN); }
			if (ad_config.tts.MakeWAVFromFile(&job)) {
				ret = AllocQueue();
				ret->path = zstrdup(api->GetBasePath());
				ret->fn = zstrdup("autodj_script.wav");
				ret->mTime = time(NULL);
				ret->ID = FileID(job.outputFN);
				ret->meta = znew(TITLE_DATA);
				ret->flags |= QUEUE_FLAG_NO_TITLE_UPDATE|QUEUE_FLAG_DELETE_AFTER;
				memcpy(ret->meta, &vd, sizeof(TITLE_DATA));
			} else {
				api->ib_printf2(pluginnum,_("AutoDJ -> Error generating AutoDJ Voice WAV file!\n"));
			}
		} else {
			job.bitrate = ad_config.Server.Bitrate;
			job.outputFN = zmprintf("%sautodj_script.mp3", api->GetBasePath());
			if (access(job.outputFN, 0) == 0) { remove(job.outputFN); }
			if (ad_config.tts.MakeMP3FromFile(&job)) {
				ret = AllocQueue();
				ret->path = zstrdup(api->GetBasePath());
				ret->fn = zstrdup("autodj_script.mp3");
				ret->mTime = time(NULL);
				ret->ID = FileID(job.outputFN);
				ret->meta = znew(TITLE_DATA);
				ret->flags |= QUEUE_FLAG_NO_TITLE_UPDATE | QUEUE_FLAG_DELETE_AFTER;
				memcpy(ret->meta, &vd, sizeof(TITLE_DATA));
			} else {
				api->ib_printf2(pluginnum,_("AutoDJ -> Error generating AutoDJ Voice MP3 file!\n"));
			}
		}
		zfreenn((void *)job.inputFN);
		zfreenn((void *)job.outputFN);
	}

	zfree(msgbuf);
	zfree(msgbuf2);

	return ret;
}

QUEUE * GetRequestedSong() {
	if (rQue == NULL) { return NULL; }

	QUEUE * Scan = rQue;
	QUEUE * lScan = NULL;
	while (Scan && Scan->isreq && Scan->isreq->playAfter > time(NULL)) {
		lScan = Scan;
		Scan = Scan->Next;
	}
	if (Scan == rQue) {
		rQue = rQue->Next;
	} else if (lScan != NULL) {
		lScan->Next = (Scan != NULL) ? Scan->Next : NULL;
	}

	return Scan;
}

void AddToPlayQueue(QUEUE * song) {
	LockMutex(QueueMutex);
	QUEUE * Scan = pQue, *lScan=NULL;
	while (Scan) {
		if (song->ID == Scan->ID) {
			api->ib_printf2(pluginnum,_("Skipping '%s', already in main playback queue...\n"), song->fn);
			FreeQueue(song);
			RelMutex(QueueMutex);
			return;
		}
		lScan = Scan;
		Scan = Scan->Next;
	}
	api->ib_printf2(pluginnum,_("Queued '%s' in main playback queue...\n"), song->fn);
	if (lScan) {
		lScan->Next = song;
	} else {
		pQue = song;
	}
	song->Next = NULL;
	RelMutex(QueueMutex);
}

QUEUE * GetNextSong() {
	QUEUE * ret = NULL;

	LockMutex(QueueMutex);

	// If a nextSong is set, play it immediately
	if (ad_config.nextSong) {
		ret = ad_config.nextSong;
		ad_config.nextSong = NULL;
		if (ret->meta && ret->meta->artist[0]) { artistList[ret->meta->artist] = time(NULL); }
		RelMutex(QueueMutex);
		return ret;
	}

	// Promo System
	if (ad_config.Options.DoPromos > 0 && ad_config.num_promos && !ad_config.cur_promo && songs_until_promo <= 0) {
		ad_config.cur_promo = ad_config.Options.NumPromos;
		songs_until_promo = api->genrand_range(ad_config.Options.DoPromos, ad_config.Options.DoPromosMax);
		if (ad_config.Options.DoPromosMax != ad_config.Options.DoPromos) {
			api->ib_printf2(pluginnum,_("AutoDJ -> Next set of promos will be in %d song(s)...\n"), songs_until_promo);
		}
	}
	if (ad_config.force_promo && ad_config.num_promos) {
		ad_config.cur_promo = 1;
		ad_config.force_promo = false;
	}
	if (ad_config.cur_promo > 0) {
		if (promos.size() == 0) {
			RelMutex(QueueMutex);
			QueuePromoFiles(ad_config.Server.PromoDir);
			LockMutex(QueueMutex);
		}
		if (promos.size() > 0) {
			ret = *promos.begin();
			promos.erase(promos.begin());
			if (ret->meta && ret->meta->artist[0]) { artistList[ret->meta->artist] = time(NULL); }
			ad_config.cur_promo--;
			RelMutex(QueueMutex);
			api->ib_printf2(pluginnum,_("AutoDJ -> Playing promo %d of %d: %s\n"),ad_config.cur_promo,ad_config.Options.NumPromos,ret->fn);
			return ret;
		}
		api->ib_printf2(pluginnum,_("AutoDJ -> Error playing promos, no promos found!\n"));
		ad_config.cur_promo=0;
	}

	// Requested song(s)
	ret = GetRequestedSong();

	if (!ret && pQue == NULL && ad_config.Rotation && ad_config.Rotation->on_need_songs) {
		//load pQue hook
		ad_config.Rotation->on_need_songs();
	}

	if (!ret && pQue) {
		ret = pQue;
		pQue = pQue->Next;
	}

	int tries=100;
	while ((songs_until_next_playlist <= 0 || current_playlist < 0) && tries--) {
		current_playlist++;
		if (current_playlist >= ad_config.num_playlist_orders) {
			current_playlist = 0;
		}
		songs_until_next_playlist = api->genrand_range(ad_config.PlaylistOrder[current_playlist].MinPlays, ad_config.PlaylistOrder[current_playlist].MaxPlays);
	}

	if (!ret) {
		ret = ad_config.Queue->GetRandomFile(ad_config.PlaylistOrder[current_playlist].Playlist);
		if (ret) {
			api->ib_printf2(pluginnum,_("AutoDJ -> Selected song '%s' from playlist '%s'...\n"), ret->fn, ad_config.Playlists[ad_config.PlaylistOrder[current_playlist].Playlist].Name);
			songs_until_next_playlist--;

			static int repeat_chance = 25;
			if (ad_config.Options.NoRepeatArtist <= 0 && ad_config.Filter == NULL && (repeat_chance <= 0 || (api->genrand_int32()%repeat_chance) == 0)) {
				repeat_chance--;
				if (ret->meta && ret->meta->artist[0]) {
	#if defined(DEBUG)
					api->ib_printf2(pluginnum,_("AutoDJ -> Trying to find another song by '%s' to play...\n"), ret->meta->artist);
	#endif
					QUEUE_FIND_RESULT * res = ad_config.Queue->FindByMeta(ret->meta->artist, META_ARTIST);
					if (res->num > 0) {
						for (int x=0; x < 5; x++) {
							int i = api->genrand_range(0, res->num - 1);
							if (res->results[i]->ID != ret->ID && AllowSongPlayback(res->results[i], time(NULL), NULL)) {
	#if defined(DEBUG)
								api->ib_printf2(pluginnum,_("AutoDJ -> Queued song '%s' by '%s'...\n"), res->results[i]->meta ? res->results[i]->meta->title:"", res->results[i]->meta ? res->results[i]->meta->artist:"");
	#endif
								AddToPlayQueue(CopyQueue(res->results[i]));
								repeat_chance = 25;
								break;
							}
						}
					}
					ad_config.Queue->FreeFindResult(res);
	#if defined(DEBUG)
					if (repeat_chance != 25) {
						api->ib_printf2(pluginnum,_("AutoDJ -> Could not find another song by '%s' to play...\n"), ret->meta->artist);
					}
	#endif
				}
			}
		}
		/*
		int i=5;
		while (i--) {
			QUEUE * scan = ad_config.Queue->GetRandomFile();
			if (scan == NULL) { break; }
			AddToPlayQueue(scan);
		}
		*/
	}

	if (ret && ad_config.nextSong == NULL && ad_config.Voice.DoVoice) {
		if (songs_until_voice <= 0 || (ret->isreq && ad_config.Voice.DoVoiceOnRequests)) {
			QUEUE * tmp = MakeVoiceMP3(ret);
			if (tmp) {
				ad_config.nextSong = ret;
				ret = tmp;
				songs_until_voice = api->genrand_range(ad_config.Voice.DoVoice, ad_config.Voice.DoVoiceMax);
				if (ad_config.Voice.DoVoice != ad_config.Voice.DoVoiceMax) {
					api->ib_printf2(pluginnum, _("AutoDJ -> Next voice announcement will be in %d song(s)...\n"), songs_until_voice);
				}
			}
		}
	}

	songs_until_promo--;
	songs_until_voice--;

	if (ret && ret->meta && ret->meta->artist[0]) { artistList[ret->meta->artist] = time(NULL); }
	RelMutex(QueueMutex);
	return ret;
}

int MessageHandler(unsigned int msg, char * data, int datalen) {
	//printf("MessageHandler(%u, %X, %d)\n", msg, data, datalen);

	for (int i = 0; i < ad_config.noplugins; i++) {
		if (ad_config.Plugins[i].plug && ad_config.Plugins[i].plug->message) {
			ad_config.Plugins[i].plug->message(msg, data, datalen);
		}
	}
	if (ad_config.Rotation && ad_config.Rotation->message) { ad_config.Rotation->message(msg, data, datalen); }

	switch(msg) {
		case SOURCE_C_PLAY:
			ad_config.playagainat=time(NULL);
			LockMutex(RepMutex);
			if (ad_config.repto) { UnrefUser(ad_config.repto); }
			ad_config.repto = NULL;
			RelMutex(RepMutex);
			ad_config.next = false;
			ad_config.countdown=false;
			ad_config.countdown_now=false;
			ad_config.playing=true;
			return 1;
			break;

		case SOURCE_C_NEXT:
			if (ad_config.playing && ad_config.connected) {
				ad_config.next = true;
			}
			return 1;
			break;

		case SOURCE_C_STOP:
			ad_config.playagainat=0;
			LockMutex(RepMutex);
			if (ad_config.repto) { UnrefUser(ad_config.repto); }
			ad_config.repto = NULL;
			RelMutex(RepMutex);
			ad_config.next = false;
			ad_config.countdown=false;
			ad_config.countdown_now=false;
			ad_config.playing=false;
			return 1;
			break;

		case SOURCE_IS_PLAYING:
			if (ad_config.playing && ad_config.connected) {
				return 1;
			}
			break;

		case AUTODJ_IS_LOADED:
			return 1;
			break;

		case SOURCE_QUEUE_FILE:{
				IBM_QUEUE_FILE * q = (IBM_QUEUE_FILE *)data;
				if (data != NULL && ad_config.playing && ad_config.connected) {
					QUEUE * Scan = AllocQueue();
					Scan->fn = zstrdup(q->fn);
					Scan->path = zstrdup("");
					LockMutex(QueueMutex);
					AddRequest(Scan, q->requesting_nick, q->to_front);
					RelMutex(QueueMutex);
					return 1;
				}
			}
			break;

		case SOURCE_GET_FILE_PATH:
		case AUTODJ_GET_FILE_PATH:
			if (strlen(data)) {
				LockMutex(QueueMutex);
				QUEUE * Scan=NULL;
				Scan = ad_config.Queue->FindFile(data);
				RelMutex(QueueMutex);
				int ret = 0;
				if (Scan) {
					ret = 1;
					memset(data, 0, datalen);
					snprintf(data, datalen, "%s%s", Scan->path, Scan->fn);
					FreeQueue(Scan);
				}
				return ret;
			}
			break;

		case SOURCE_GET_SONGINFO:{
				IBM_SONGINFO * si = (IBM_SONGINFO *)data;
				if (LoadSongInfo(si)) {
					return 1;
				}
				return 0;
			}
			break;

		case SOURCE_GET_SONGID:{
				LockMutex(QueueMutex);
				if (ad_config.playing && ad_config.connected && ad_config.cursong) {
					*((uint32 *)data) = ad_config.cursong->ID;
					RelMutex(QueueMutex);
					return 1;
				}
				RelMutex(QueueMutex);
			}
			break;

		case IB_PROCTEXT:{
				IBM_PROCTEXT * pt = (IBM_PROCTEXT *)data;
				char buf[256];

				bool done = false;
				if (ad_config.connected && ad_config.playing && !ad_config.reloading) {
					if (TryLockMutex(QueueMutex, 10000)) {
						if (ad_config.cursong) {
							done = true;
							sprintf(buf, "%d", GetTimeLeft(false));
							str_replace(pt->buf, pt->bufSize, "%timeleft_milli%", buf);
							sprintf(buf, "%d", GetTimeLeft(true));
							str_replace(pt->buf, pt->bufSize, "%timeleft_secs%", buf);
							if (ad_config.cursong->meta && ad_config.cursong->meta->title[0]) {
								if (ad_config.cursong->meta->artist[0]) {
									snprintf(buf, sizeof(buf), "%s - %s", ad_config.cursong->meta->artist, ad_config.cursong->meta->title);
									str_replace(pt->buf, pt->bufSize, "%adjsong%", buf);
								} else {
									str_replace(pt->buf, pt->bufSize, "%adjsong%", ad_config.cursong->meta->title);
								}
							} else {
								str_replace(pt->buf, pt->bufSize, "%adjsong%", ad_config.cursong->fn);
							}
							str_replace(pt->buf, pt->bufSize, "%adjfile%", ad_config.cursong->fn);
						}
						RelMutex(QueueMutex);
					}
				}

				if (!done) {
					str_replace(pt->buf, pt->bufSize, "%timeleft_milli%", "0");
					str_replace(pt->buf, pt->bufSize, "%timeleft_secs%", "0");
					str_replace(pt->buf, pt->bufSize, "%adjsong%", "Not Playing");
					str_replace(pt->buf, pt->bufSize, "%adjfile%", "Not Playing");
				}

				return 0;
			}
			break;

		case IB_ON_SONG_RATING:{
				IBM_RATING * ir = (IBM_RATING *)data;
				LockMutex(QueueMutex);
				if (ir->AutoDJ_ID && ad_config.Queue && ad_config.Queue->on_song_rating) {
					ad_config.Queue->on_song_rating(ir->AutoDJ_ID, ir->rating, ir->votes);
				}
				RelMutex(QueueMutex);
			}
			break;

		case IB_SS_DRAGCOMPLETE:{
				STATS * s = api->ss->GetStreamInfo();
				static time_t lastOffline = 0;
				if (ad_config.Options.AutoPlayIfNoSource && !s->online && !ad_config.playing && time(NULL) >= ad_config.playagainat) {
					if (lastOffline == 0) {
						lastOffline = time(NULL);
					} else if ((time(NULL) - lastOffline) > ad_config.Options.AutoPlayIfNoSource){
						api->ib_printf2(pluginnum,_("AutoDJ -> Auto-starting, stream has no source...\n"));
						if (api->irc) { api->irc->LogToChan(_("AutoDJ -> Auto-starting, stream has no source...")); }
						ad_config.playing = true;
					}
				} else if (s->online) {
					lastOffline = 0;
				}
			 }
			break;
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{9E8E8536-0142-4950-9892-F14FCCB1AF65}",
	"AutoDJ",
	"AutoDJ Plugin " AUTODJ_VERSION "",
	"Drift Solutions",
	init,
	quit,
	MessageHandler,
	NULL,
	NULL,
	NULL,
	remote,
};

PLUGIN_EXPORT_FULL

DL_HANDLE getInstance() {
	return plugin.hInstance;
}

