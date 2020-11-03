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
#if defined(WIN32)
#include "resource.h"
#endif

DECODER * GetFileDecoder(const char * fn) {
	DECODER * ret = NULL;
	for (int dsel=0; ret == NULL && ad_config.Decoders[dsel] != NULL; dsel++) {
		try {
			if (ad_config.Decoders[dsel]->is_my_file(fn, NULL)) {
				ret = ad_config.Decoders[dsel];
			}
		} catch(...) {
			ret = NULL;
		}
	}
	DRIFT_DIGITAL_SIGNATURE();
	return ret;
}

/*
int PlaySongToSock(const char * buf, TITLE_DATA * meta=NULL, int64 fromPos=0) {
	char buf2[1024];
	sprintf(buf2,_("AutoDJ -> Current File: '%s'\n"),buf);
	api->LogToChan(buf2);
	api->ib_printf2(pluginnum,buf2);

	ad_config.resumePos = 0;
	READER_HANDLE * fp = OpenFile(buf, "rb");

	for (int dsel=0; ad_config.Decoders[dsel] != NULL; dsel++) {
		if (fp && ad_config.Decoders[dsel]->is_my_file(buf, fp ? fp->mime_type : NULL)) {
			fp->seek(fp, 0, SEEK_SET);

			if (ad_config.Decoders[dsel]->create) {
				AutoDJ_Decoder * dec = ad_config.Decoders[dsel]->create();
				if (dec->Open(fp, meta, fromPos) || dec->Open_URL(buf, meta, fromPos)) {
					int ret = dec->Decode();
					while (ret == AD_DECODE_CONTINUE) {
						ret = dec->Decode();
					}
					if (ret == AD_DECODE_ERROR) {
						ad_config.resumePos = dec->GetPosition();
					}
					dec->Close();
					ad_config.Decoders[dsel]->destroy(dec);
					fp->close(fp);
					return ret;
				}
				ad_config.Decoders[dsel]->destroy(dec);
			}
		}
	}

	if (fp) { fp->close(fp); }
	sprintf(buf2,_("AutoDJ -> Could not find decoder for '%s'\n"),buf);
	api->LogToChan(buf2);
	api->ib_printf2(pluginnum,buf2);
	return AD_DECODE_DONE;
}
*/

bool ConnectFeeder() {
	API_SS ss;
	bool ret = false;

	api->ss->GetSSInfo(0,&ss);
	if ((ss.type == SS_TYPE_ICECAST || ss.type == SS_TYPE_STEAMCAST) && !strlen(ad_config.Server.Mount)) {
		sstrcpy(ad_config.Server.Mount, ss.mount);
		if (!strlen(ad_config.Server.Mount)) {
			api->ib_printf2(pluginnum,_("AutoDJ -> Error: No Mountpoint specified for server! Falling back to /\n"));
			strcpy(ad_config.Server.Mount, "/");
		}
	}
	if (!ad_config.Feeder) {
		for (int i=0; feeders[i].create != NULL; i++) {
			if (ss.type == feeders[i].type) {
				ad_config.SelFeeder = &feeders[i];
				ad_config.Feeder = feeders[i].create();
				break;
			}
		}
	}
	if (ad_config.Feeder) {
		if (ad_config.Feeder->IsConnected() || ad_config.Feeder->Connect()) {
			ret = true;
		}
	}
	return ret;
}

void DisconnectFeeder() {
	if (ad_config.Feeder) {
		if (ad_config.Feeder->IsConnected()) {
			ad_config.Feeder->Close();
		}
	}
}

#if defined(WIN32)
int last_icon = -1;
void RemoveTaskbarIcon() {
	if (ad_config.pTaskList) {
		ad_config.pTaskList->SetOverlayIcon(api->hWnd, NULL, L"AutoDJ Playback Status");
		last_icon = -1;
	}
}
void SetTaskbarIcon(int icon) {
	if (ad_config.pTaskList) {
		if (last_icon != icon) {
			//ad_config.pTaskList->SetOverlayIcon(api->hWnd, NULL, L"AutoDJ Playback Status");
			HICON hLoad = (HICON)LoadImage(getInstance(), MAKEINTRESOURCE(icon), IMAGE_ICON, 16, 16, 0);
			if (ad_config.pTaskList->SetOverlayIcon(api->hWnd, hLoad, L"AutoDJ Playback Status") == S_OK) {
				last_icon = icon;
			}
			DestroyIcon(hLoad);
		}
	}
}
#endif

THREADTYPE PlayThread(VOID *lpData) {
	TT_THREAD_START

	char buf[1024*2];//,buf2[1024*2];

	QueueContentFiles();

//	printf("AutoDJ -> Play Thread Started\n");
	#if defined(WIN32)
	bool have_com = true;
	if (FAILED(CoInitialize(NULL))) {
		have_com = false;
	    api->ib_printf2(pluginnum,_("Error intiliazing COM!\n"));
    }
	#else
	signal(SIGSEGV,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	#endif

	for (;!ad_config.shutdown_now;) {
#if defined(WIN32)
		if (ad_config.pTaskList) {
			if (ad_config.playing) {
				ad_config.pTaskList->SetProgressState(api->hWnd, TBPF_PAUSED);
				SetTaskbarIcon(IDI_PAUSE);
			} else {
				ad_config.pTaskList->SetProgressState(api->hWnd, TBPF_NOPROGRESS);
				SetTaskbarIcon(IDI_STOP);
			}
		}
#endif
		api->safe_sleep_seconds(1);
		if (ad_config.shutdown_now) { break; }
		api->safe_sleep_seconds(1);
		if (ad_config.shutdown_now) { break; }
		api->safe_sleep_seconds(1);
		if (ad_config.shutdown_now) { break; }
		api->safe_sleep_seconds(1);
		if (ad_config.shutdown_now) { break; }

		if (ad_config.playagainat > 0) {
			if (ad_config.playagainat < time(NULL)) {
				ad_config.playing=true;
				ad_config.playagainat=0;
			}
		}

		if (ad_config.playing && !ad_config.shutdown_now && (ad_config.num_files > 0 || rQue || pQue || ad_config.nextSong)) {
			api->ib_printf2(pluginnum,_("AutoDJ -> Attempting to connect to Sound Server(s)...\n"));

			if (ConnectFeeder()) {
				if (GetMixer()->init()) {
					ad_config.connected = true;
					api->SendMessage(-1, SOURCE_I_PLAYING, NULL, 0);
					if (ad_config.Options.EnableRequests) {
						api->ss->EnableRequestsHooked(&adj_req_iface);
					}
					time_t tStart = time(NULL) - 5; // send 10 seconds when we connect to help prevent buffering

#if defined(WIN32)
					if (ad_config.pTaskList) {
						ad_config.pTaskList->SetProgressState(api->hWnd, TBPF_NORMAL);
						ad_config.pTaskList->SetProgressValue(api->hWnd, 0, 1);
						SetTaskbarIcon(IDI_PLAY);
					}
#endif
					while (ad_config.connected && ad_config.playing && !ad_config.shutdown_now) {
						if (ad_config.Options.EnableRequests && api->ss->GetRequestMode() != REQ_MODE_HOOKED) {
							api->ss->EnableRequestsHooked(&adj_req_iface);
						}
						if (GetMixer()->getOutputTime() < ((time(NULL) - tStart + 1)*1000)) {
							#if defined(DEBUG)
							//printf("outputTime: "U64FMT"\n", GetMixer()->getOutputTime());
							#endif
#if defined(WIN32)
							SetTaskbarIcon(IDI_PLAY);
#endif
							if (!GetMixer()->encode()) {
								ad_config.connected = false;
							}
						} else {
							safe_sleep(100,true);
						}
					} // while (ad_config.connected)

					if (ad_config.connected) {
						UnloadActiveDeck();
					}

					api->SendMessage(-1, SOURCE_I_STOPPING, NULL, 0);

					api->ib_printf2(pluginnum,_("AutoDJ -> Disconnecting from SS...\n"),buf);
					api->ss->EnableRequests(false);
					ad_config.connected=false;
					GetMixer()->close();
					api->SendMessage(-1, SOURCE_I_STOPPED, NULL, 0);
				} else {
					api->ib_printf2(pluginnum,_("AutoDJ -> Error initializing mixer/encoder!\n"));
				}
				DisconnectFeeder();
			} else {
				api->ib_printf2(pluginnum,_("AutoDJ -> Error connecting to Sound Server as source!\n"));
			}
		}
	}

	#if defined(WIN32)
	RemoveTaskbarIcon();
	if (ad_config.pTaskList) {
		ad_config.pTaskList->SetProgressState(api->hWnd, TBPF_NOPROGRESS);
	}
	if (have_com) {
		CoUninitialize();
	}
	#endif
	api->ib_printf2(pluginnum,_("AutoDJ -> Play Thread Stopped\n"));
	TT_THREAD_END
}
