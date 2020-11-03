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
typedef std::vector<TITLE_DATA *> titleUpdates;
titleUpdates titles;
void QueueTitleUpdate(TITLE_DATA * td) {
	TitleMutex.Lock();
	TITLE_DATA * td2 = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
	memcpy(td2,td,sizeof(TITLE_DATA));
	titles.push_back(td2);
	TitleMutex.Release();
}

void urlencode(char * buf, unsigned long bufSize) {
	str_replace(buf,bufSize," ","%20");
	str_replace(buf,bufSize,"&","%26");
	str_replace(buf,bufSize,"'","%27");
	str_replace(buf,bufSize,"[","%5B");
	str_replace(buf,bufSize,"]","%5D");
}

/*
					api->ib_printf("SimpleDJ -> [stream-%02d] Connecting to SS to send update...\n", j);
					if (sockets->ConnectWithTimeout(sockupd,sc.host,sc.port,30000)) {
						sockets->SetNonBlocking(sockupd,true);
						api->ib_printf("SimpleDJ -> [stream-%02d] Sending title update... (%s/%s)\n",j, doTitle->artist, doTitle->title);
						sockets->Send(sockupd,url,strlen(url));
						api->safe_sleep_seconds(1);
					} else {
						api->ib_printf("SimpleDJ -> [stream-%02d] Error sending title update, could not connect to server...\n", j);
					}
					sockets->Close(sockupd);
*/

THREADTYPE UpdateTitle(VOID *lpData) {
	TT_THREAD_START
	#ifndef _WIN32
	signal(SIGSEGV,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	#endif
	api->ib_printf("SimpleDJ -> Title Update Thread Started...\n");
	//API_SS sc;
	//char url[1024],url2[1024];

	YP_HANDLE yp_handle;
	memset(&yp_handle, 0, sizeof(yp_handle));
	time_t yp_last_try = 0;

	while (!sd_config.shutdown_now) {
		api->safe_sleep_seconds(1);
		TITLE_DATA * doTitle = NULL;
		TitleMutex.Lock();
		titleUpdates::iterator x = titles.begin();
		if (x != titles.end()) {
			doTitle = *x;
			titles.erase(x);
		}
		TitleMutex.Release();

		if (doTitle != NULL) {
			TITLE_DATA td2;
			memcpy(&td2,doTitle,sizeof(TITLE_DATA));
			if (strlen(doTitle->artist)) {
				str_replace(doTitle->artist,sizeof(doTitle->artist)," ","%20");
				str_replace(doTitle->artist,sizeof(doTitle->artist),"&","%26");
				str_replace(doTitle->artist,sizeof(doTitle->artist),"'","%27");
				str_replace(doTitle->artist,sizeof(doTitle->artist),"[","%5B");
				str_replace(doTitle->artist,sizeof(doTitle->artist),"]","%5D");
				str_replace(doTitle->artist,sizeof(doTitle->artist),"_","%20");
			}
			str_replace(doTitle->title,sizeof(doTitle->title)," ","%20");
			str_replace(doTitle->title,sizeof(doTitle->title),"&","%26");
			str_replace(doTitle->title,sizeof(doTitle->title),"'","%27");
			str_replace(doTitle->title,sizeof(doTitle->title),"[","%5B");
			str_replace(doTitle->title,sizeof(doTitle->title),"]","%5D");
			str_replace(doTitle->title,sizeof(doTitle->title),"_","%20");

			//api->GetSSInfo(0,&sc);

			if (sd_config.Options.EnableYP && sd_config.playing && sd_config.connected && (time(NULL) - yp_last_try) >= 1) {
				stringstream sstr;
				if (strlen(td2.artist)) {
					sstr << td2.artist << " - ";
				}
				sstr << td2.title;
				if (yp_handle.yp_id) {
					if (api->yp->TouchYP(&yp_handle, sstr.str().c_str()) == -2) {
						memset(&yp_handle, 0, sizeof(yp_handle));
					}
				} else {
					API_SS sc;
					api->ss->GetSSInfo(0,&sc);
					YP_CREATE_INFO yp_info;
					memset(&yp_info, 0, sizeof(yp_info));
					yp_info.source_name = "AutoDJ";
					string title = sstr.str();
					yp_info.cur_playing = title.c_str();
					yp_info.genre = sd_config.Server.Genre;
					yp_info.mime_type = sd_config.Server.MimeType;
					if (!api->yp->AddToYP(&yp_handle, &sc, &yp_info)) {
						memset(&yp_handle, 0, sizeof(yp_handle));
					}
				}
				yp_last_try = time(NULL);
			}

			stringstream sstr;
			if (strlen(doTitle->artist)) {
				sstr << doTitle->artist << "%20-%20";
			}
			sstr << doTitle->title;

			for (int i=0; i < MAX_SOUND_SERVERS; i++) {
				if (sd_config.Servers[i].Enabled && sd_config.Servers[i].Feeder && sd_config.Servers[i].Feeder->IsConnected()) {
					//bool diderr = false;
					sd_config.Servers[i].Feeder->SendTitleUpdate(sstr.str().c_str(), &td2);
				}
			}

			zfree(doTitle);
		} else if (sd_config.Options.EnableYP && sd_config.playing && sd_config.connected && yp_handle.yp_id && (time(NULL) - yp_last_try) >= 300) {
			if (api->yp->TouchYP(&yp_handle, NULL) == -2) {
				memset(&yp_handle, 0, sizeof(yp_handle));
			}
			yp_last_try = time(NULL);
		}

		if ((!sd_config.Options.EnableYP || !sd_config.playing || !sd_config.connected || sd_config.shutdown_now) && yp_handle.yp_id) {
			api->yp->DelFromYP(&yp_handle);
			memset(&yp_handle, 0, sizeof(yp_handle));
		}
	}

	while (sd_config.connected) {
		safe_sleep(100, true);
	}

	TitleMutex.Lock();
	for (titleUpdates::iterator x = titles.begin(); x != titles.end(); x++) {
		TITLE_DATA * toDel = *x;
		zfree(toDel);
	}
	titles.clear();
	TitleMutex.Release();

	if (yp_handle.yp_id) {
		api->yp->DelFromYP(&yp_handle);
	}

	api->ib_printf("SimpleDJ -> Title Update Thread Exiting...\n");
	TT_THREAD_END
}

