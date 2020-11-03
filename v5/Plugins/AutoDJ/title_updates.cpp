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

Titus_Mutex TitleMutex;
struct TITLE_DATA_QUEUE {
	TITLE_DATA update;
	int tries;
};
typedef std::vector<TITLE_DATA_QUEUE *> titleUpdates;
titleUpdates titles;
void QueueTitleUpdate(TITLE_DATA * td) {
	TitleMutex.Lock();
	TITLE_DATA_QUEUE * q = znew(TITLE_DATA_QUEUE);
	memset(q, 0, sizeof(TITLE_DATA_QUEUE));
	q->update = *td;
	q->tries = 5;
	titles.push_back(q);
	TitleMutex.Release();
}

char * urlencode(const char * in, char * out, unsigned long bufSize) {
	char * tmp = api->curl->escape(in, strlen(in));
	if (tmp) {
		strlcpy(out, tmp, bufSize);
		api->curl->free(tmp);
	} else {
		strlcpy(out, in, bufSize);
		str_replace(out,bufSize," ","%20");
		str_replace(out,bufSize,"&","%26");
		str_replace(out,bufSize,"'","%27");
		str_replace(out,bufSize,"[","%5B");
		str_replace(out,bufSize,"]","%5D");
	}
	return out;
}

inline char * urlencode(char * buf, unsigned long bufSize) {
	return urlencode(buf, buf, bufSize);
}

THREADTYPE UpdateTitle(VOID *lpData) {
	TT_THREAD_START
	#ifndef _WIN32
	signal(SIGSEGV,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	#endif
	api->ib_printf2(pluginnum, "AutoDJ -> Title Update Thread Started...\n");
	//API_SS sc;
	char url[1024];

	YP_HANDLE yp_handle;
	memset(&yp_handle, 0, sizeof(yp_handle));
	time_t yp_last_try = 0;

	while (!ad_config.shutdown_now) {
		api->safe_sleep_seconds(1);
		TITLE_DATA_QUEUE * doTitle = NULL;
		TitleMutex.Lock();
		if (titles.size() > 0) {
			doTitle = *titles.begin();
		}
		TitleMutex.Release();

		if (doTitle != NULL) {
			/*
			TITLE_DATA td2;
			memcpy(&td2,doTitle,sizeof(TITLE_DATA));
			if (ad_config.Options.StrictParse) {
				str_replace(doTitle->title,sizeof(doTitle->title),"-"," ");
				str_replace(doTitle->artist,sizeof(doTitle->artist),"-"," ");
				str_replace(doTitle->album,sizeof(doTitle->album),"-"," ");
			}
			if (doTitle->artist[0]) {
				urlencode(doTitle->artist, sizeof(doTitle->artist));
			}
			if (doTitle->album[0]) {
				urlencode(doTitle->album, sizeof(doTitle->album));
			}
			urlencode(doTitle->title, sizeof(doTitle->title));
			*/

			//api->GetSSInfo(0,&sc);

			if (ad_config.Options.EnableYP && ad_config.playing && ad_config.connected && (time(NULL) - yp_last_try) >= 10) {
				stringstream sstr;
				if (strlen(doTitle->update.artist)) {
					sstr << doTitle->update.artist << " - ";
				}
				sstr << doTitle->update.title;
				if (yp_handle.yp_id) {
					if (api->yp->TouchYP(&yp_handle, sstr.str().c_str()) == -2) {
						memset(&yp_handle, 0, sizeof(yp_handle));
					}
					yp_last_try = time(NULL);
				} else {
					API_SS sc;
					api->ss->GetSSInfo(0,&sc);
					YP_CREATE_INFO yp_info;
					memset(&yp_info, 0, sizeof(yp_info));
					yp_info.source_name = "AutoDJ";
					string title = sstr.str();
					yp_info.cur_playing = title.c_str();
					yp_info.genre = ad_config.Server.Genre;
					yp_info.mime_type = ad_config.Server.MimeType;
					if (!api->yp->AddToYP(&yp_handle, &sc, &yp_info)) {
						memset(&yp_handle, 0, sizeof(yp_handle));
					}
					yp_last_try = time(NULL);
				}
			}

			if (ad_config.Feeder && ad_config.Options.EnableTitleUpdates) {
				stringstream sstr;
				if (doTitle->update.artist[0]) {
					sstr << doTitle->update.artist << " - ";
					if (ad_config.Options.IncludeAlbum && doTitle->update.album[0]) {
						sstr << doTitle->update.album << " - ";
					}
				}
				sstr << doTitle->update.title;
				//bool diderr = false;
				api->ib_printf2(pluginnum, "AutoDJ -> Sending title update... (%s)\n", sstr.str().c_str());
				urlencode(sstr.str().c_str(), url, sizeof(url));
				if (ad_config.Feeder->SendTitleUpdate(url, &doTitle->update)) {
					if (ad_config.Encoder != NULL) {
						ad_config.Encoder->OnTitleUpdate(url, &doTitle->update);
					}
					doTitle->tries = 0;
				} else {
					doTitle->tries--;
					if (doTitle->tries > 0) {
						api->safe_sleep_seconds(4);
					}
				}
			}

			if (doTitle->tries <= 0) {
				zfree(doTitle);
				TitleMutex.Lock();
				titles.erase(titles.begin());
				TitleMutex.Release();
			}
		} else if (ad_config.Options.EnableYP && ad_config.playing && ad_config.connected && yp_handle.yp_id && (time(NULL) - yp_last_try) >= 300) {
			if (api->yp->TouchYP(&yp_handle, NULL) == -2) {
				memset(&yp_handle, 0, sizeof(yp_handle));
			}
			yp_last_try = time(NULL);
		}

		if ((!ad_config.Options.EnableYP || !ad_config.playing || !ad_config.connected || ad_config.shutdown_now) && yp_handle.yp_id) {
			api->yp->DelFromYP(&yp_handle);
			memset(&yp_handle, 0, sizeof(yp_handle));
		}
	}

	while (ad_config.connected) {
		safe_sleep(100, true);
	}

	TitleMutex.Lock();
	for (titleUpdates::iterator x = titles.begin(); x != titles.end(); x++) {
		TITLE_DATA_QUEUE * toDel = *x;
		zfree(toDel);
	}
	titles.clear();
	TitleMutex.Release();

	if (yp_handle.yp_id) {
		api->yp->DelFromYP(&yp_handle);
		memset(&yp_handle, 0, sizeof(yp_handle));
	}

	api->ib_printf2(pluginnum,"AutoDJ -> Title Update Thread Exiting...\n");
	TT_THREAD_END
}

