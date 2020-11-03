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

#define COMCTL32_V6
#include "client.h"
#include "../Common/remote_protobuf.pb.h"

#if defined(DEBUG)
#pragma comment(lib, "libprotobuf_d.lib")
#pragma comment(lib, "sqlite3_d.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#pragma comment(lib, "sqlite3.lib")
#endif

#define IRCBOT_VERSION_FLAG_DEBUG		0x00000001
#define IRCBOT_VERSION_FLAG_LITE		0x00000002
#define IRCBOT_VERSION_FLAG_BASIC		0x00000004
#define IRCBOT_VERSION_FLAG_FULL		0x00000008
#define IRCBOT_VERSION_FLAG_STANDALONE	0x00000010

CONFIG config;
RemoteClient * rc = NULL;
Titus_Mutex hostMutex;

void DoConnect(int newindex) {
	if (!strlen(config.hosts[newindex].name) || !strlen(config.hosts[newindex].host) || !strlen(config.hosts[newindex].username) || !strlen(config.hosts[newindex].pass)) {
		if (MessageBox(config.mWnd, TEXT("You have not set up your connection!\n\nWould you like to open the Connection Manager now?"), TEXT("Error"), MB_YESNO|MB_ICONERROR) == IDYES) {
			OpenConnManager(config.mWnd);
		}
		return;
	}

	AutoMutex(hostMutex);
	config.host_index = newindex;
	config.connect = true;
}
bool IsConnected() {
	return rc->IsReady();
}

void UpdateTitle() {
	ostringstream sstr;
	sstr << "RadioBot Client v" VERSION "";
	if (rc->IsReady()) {
		sstr << " - Logged in as " << config.hosts[config.host_index].username << " (";
		if (rc->IsIRCBotv5()) {
			char buf[64];
			uflag_to_string(rc->GetUserFlags(), buf, sizeof(buf));
			sstr << buf;
		} else {
			sstr << "Level " << rc->GetUserLevel();
		}
		sstr << ")";
	}
	SetWindowText(config.mWnd, sstr.str().c_str());
}

INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
void SwitchTab(int num) {
	if (config.tWnd[num] == NULL) {
		RECT rc;
		GetWindowRect(GetDlgItem(config.mWnd, IDC_TABFRAME), &rc);
		POINT pt;
		pt.x = rc.left;
		pt.y = rc.top;
		ScreenToClient(config.mWnd, &pt);
		switch (num) {
			case TAB_DJ:
				config.tWnd[num] = CreateTabDJ(config.mWnd);
				break;
			case TAB_FIND:
				config.tWnd[num] = CreateTabFind(config.mWnd);
				break;
			case TAB_ADMIN:
				config.tWnd[num] = CreateTabAdmin(config.mWnd);
				break;
			case TAB_OPTIONS:
				config.tWnd[num] = CreateTabOptions(config.mWnd);
				break;
			case TAB_ABOUT:
				config.tWnd[num] = CreateTabAbout(config.mWnd);
				break;
			case TAB_HELP:
				config.tWnd[num] = CreateTabHelp(config.mWnd);
				break;
		}
		//MoveWindow(config.tWnd[num], rc.top, rc.left, 0, 0, TRUE);
		SetWindowPos(config.tWnd[num], HWND_TOP, pt.x, pt.y, rc.right - rc.left, rc.bottom - rc.top, 0);
	}
	for (int i=0; i < NUM_TABS; i++) {
		if (config.tWnd[i]) {
			if (i != num) {
				ShowWindow(config.tWnd[i], SW_HIDE);
			} else {
				ShowWindow(config.tWnd[i], SW_SHOW);
			}
		}
	}
}

void SetWindowOnTop() {
	DWORD dwStyle = GetWindowLong(config.mWnd, GWL_EXSTYLE);
	if (config.keep_on_top) {
		dwStyle |= WS_EX_TOPMOST;
		SetWindowLong(config.mWnd, GWL_EXSTYLE, dwStyle);
		SetWindowPos(config.mWnd,HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
	} else {
		dwStyle &= ~WS_EX_TOPMOST;
		SetWindowLong(config.mWnd, GWL_EXSTYLE, dwStyle);
		SetWindowPos(config.mWnd,HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
	}
}

#ifndef ICC_STANDARD_CLASSES
#define ICC_STANDARD_CLASSES   0x00004000
#endif

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	/*
	INITCOMMONCONTROLSEX initCmnCtrls;
	initCmnCtrls.dwSize = sizeof(initCmnCtrls);
	initCmnCtrls.dwICC = ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&initCmnCtrls);
	*/
	LoadLibrary("Riched20.dll");

	CoInitialize(NULL);
	memset(&config, 0, sizeof(config));
	config.hInst = hInstance;
	config.beep_on_req = true;

	rc = new RemoteClient();
	LoadConfig();
	MusicDB_Init();

	CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DlgProc);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		if (!IsDialogMessage(config.mWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	SaveConfig();
	MusicDB_Quit();
	delete rc;

	CoUninitialize();
	return 0;
}

const char * tab_names[NUM_TABS] = {
	"DJ / Request System",
	"@find Support",
	"Admin Functions",
	"Options",
	"About",
	"Help"
};

TT_DEFINE_THREAD(ConxThread);
WM_HANDLER(OnInitDialog) {
	config.mWnd = hWnd;
	UpdateTitle();

	TabCtrl_DeleteAllItems(GetDlgItem(hWnd, IDC_TABS));
	TCITEM tab;
	int ind=0;
	memset(&tab, 0, sizeof(tab));
	tab.mask = TCIF_TEXT;
	for (int i=0; i < NUM_TABS; i++) {
		if (i != TAB_HELP || access("client_help.txt", 0) == 0) {
			tab.pszText = (LPSTR)tab_names[i];
			TabCtrl_InsertItem(GetDlgItem(hWnd, IDC_TABS), ind++, &tab);
		}
	}

	RECT rc;
	GetWindowRect(GetDlgItem(hWnd, IDC_TABFRAME), &rc);
	rc.top = rc.top;

	TabCtrl_SetCurSel(GetDlgItem(hWnd, IDC_TABS), 0);
	SwitchTab(0);
	SetWindowOnTop();
	ShowWindow(hWnd, SW_SHOW);

	if (config.connect_on_startup) {
		config.connect = true;
	}
	TT_StartThread(ConxThread, NULL, "RadioBot Connection Thread");
	return TRUE;
}

WM_HANDLER(OnClose) {
	if (!config.shutdown_now) {
		config.shutdown_now = true;
		SetTimer(hWnd, 1, 100, NULL);
		SetDlgItemText(hWnd, IDC_RESPONSE, "Waiting for threads to exit...");
	}
	if (TT_NumThreads() > 0) {
		return TRUE;
	}
	KillTimer(hWnd, 1);

	DestroyWindow(hWnd);
	PostQuitMessage(0);
	return TRUE;
}

WM_HANDLER(OnCommand) {
	if (LOWORD(wParam) == IDC_CONNMGR) {
		OpenConnManager(hWnd);
		return TRUE;
	} else if (LOWORD(wParam) == IDC_CONNECT) {
		if (config.host_index >= 0 && config.hosts[config.host_index].name[0]) {
			config.connect = config.connect ? false:true;
		} else {
			MessageBox(hWnd, TEXT("You do not have a connection selected or have not created a connection yet!"), TEXT("Error"), MB_ICONERROR);
		}
	}
	return FALSE;
}

WM_HANDLER(OnTimer) {
	if (wParam == 1) {
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		return TRUE;
	}
	return FALSE;
}

WM_HANDLER(OnUser) {
	if (wParam == 0xDEADBEEF) {
		SetDlgItemTextA(hWnd, IDC_RESPONSE, (LPCSTR)lParam);
		return TRUE;
	} else if (wParam == 0xDEADBEEB) {
		UpdateTitle();
		int imgid = IDI_DISCONNECTED;
		switch (lParam) {
			case -1:
				SetDlgItemText(hWnd, IDC_CONNECT, TEXT("Connect"));
				SetDlgItemTextA(hWnd, IDC_RESPONSE, "Disconnected");
				break;
			case 0:
				SetDlgItemText(hWnd, IDC_CONNECT, TEXT("Connect"));
				break;
			case 1:
				imgid = IDI_CONNECTING;
				SetDlgItemText(hWnd, IDC_CONNECT, TEXT("Abort"));
				SetDlgItemTextA(hWnd, IDC_RESPONSE, "Connecting...");
				break;
			case 2:
				imgid = IDI_CONNECTED;
				SetDlgItemText(hWnd, IDC_CONNECT, TEXT("Disconnect"));
				break;
		}
		HICON hIcon = (HICON)LoadImage(config.hInst, MAKEINTRESOURCE(imgid), IMAGE_ICON, 0, 0, LR_SHARED);
		HICON hOldIcon = (HICON)SendDlgItemMessage(hWnd, IDC_CONNIMG, STM_SETICON, (WPARAM)hIcon, 0);
		if (hOldIcon) {
			//DestroyIcon(hOldIcon);
		}
	} else if (wParam == 0xDEADBEBE) {
		char buf[256];
		BOT_REPLY * br = (BOT_REPLY *)lParam;
		if (br->rHead.scmd == RCMD_STREAM_INFO) {
			STREAM_INFO * si = (STREAM_INFO *)br->data;
			si->dj[sizeof(si->dj)-1] = 0;
			SetDlgItemText(hWnd, IDC_STREAM_DJ, si->dj);
			si->title[sizeof(si->title)-1] = 0;
			SetDlgItemText(hWnd, IDC_STREAM_TITLE, si->title);
			sprintf(buf, TEXT("Listeners: %d/%d, Peak: %d"), si->listeners, si->max, si->peak);
			SetDlgItemText(hWnd, IDC_STREAM_STATS, buf);
		} else if (br->rHead.scmd == RCMD_GENERIC_MSG || br->rHead.scmd == RCMD_GENERIC_ERROR) {
			SetDlgItemText(hWnd, IDC_RESPONSE, br->data);
		} else if (br->rHead.scmd == RCMD_REQ_LOGIN_ACK) {
			SetDlgItemText(hWnd, IDC_RESPONSE, TEXT("You are now the active DJ!"));
		} else if (br->rHead.scmd == RCMD_REQ_LOGOUT_ACK) {
			SetDlgItemText(hWnd, IDC_RESPONSE, TEXT("You are no longer the active DJ."));
		} else if (br->rHead.scmd == RCMD_REQ_INCOMING) {
			char * p2 = NULL;
			char * p = strtok_r(br->data, "\xFE", &p2);
			while (p) {
				ListBox_AddString(GetDlgItem(config.tWnd[TAB_DJ], IDC_DJ_REQS), p);
				p = strtok_r(NULL, "\xFE", &p2);
			}
			#pragma comment(lib, "winmm.lib")
			if (config.beep_on_req) {
				if (access("incoming_req.wav",0) == 0) {
					PlaySound(TEXT("incoming_req.wav"), NULL, SND_FILENAME|SND_ASYNC);
				} else {
					PlaySound(MAKEINTRESOURCE(IDR_WAVE1), GetModuleHandle(NULL), SND_RESOURCE|SND_ASYNC);
				}
			}
		} else if (br->rHead.scmd == RCMD_IRCBOT_VERSION) {
			uint32 ver = *((uint32 *)br->data);
			const char * type = "Unknown";
			if (ver & IRCBOT_VERSION_FLAG_FULL) {
				type = "Full";
			} else if (ver & IRCBOT_VERSION_FLAG_BASIC) {
				type = "Basic";
			} else if (ver & IRCBOT_VERSION_FLAG_STANDALONE) {
				type = "AutoDJ";
			} else if (ver & IRCBOT_VERSION_FLAG_LITE) {
				type = "Lite";
			}
			snprintf(buf, sizeof(buf), "RadioBot Version: %u.%02u %s", (ver >> 16)&0xFF, (ver >> 8)&0xFF, type);
			SetDlgItemText(hWnd, IDC_RESPONSE, buf);
		} else if (br->rHead.scmd == RCMD_USERINFO) {
			if (config.tWnd[TAB_ADMIN] != NULL) {
				char buf[64];
				IBM_USEREXTENDED * ue = (IBM_USEREXTENDED *)br->data;
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_USERNAME, ue->nick);
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_PASS, ue->pass);
				uflag_to_string(ue->flags, buf, sizeof(buf));
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_FLAGS, buf);
				sprintf(buf, "%d", ue->numhostmasks);
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_NUMHM, buf);
			}
		} else if (br->rHead.scmd == RCMD_USERNOTFOUND) {
			if (config.tWnd[TAB_ADMIN] != NULL) {
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_USERNAME, "User not found!");
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_PASS, "");
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_FLAGS, "");
				SetDlgItemText(config.tWnd[TAB_ADMIN], IDC_ADMIN_NUMHM, "0");
			}
		} else if (br->rHead.scmd == RCMD_FIND_QUERY) {
			RemoteFindQuery q;
			if (q.ParseFromArray(br->data, br->rHead.datalen) && q.IsInitialized()) {
				RemoteFindResult res;
				res.set_search_id(q.search_id());
				FIND_RESULTS * qRes = MusicDB_Find(q.query().c_str(), q.search_id());
				if (qRes) {
					for (int i=0; i < qRes->num_songs; i++) {
						if (res.ByteSize() > MAX_REMOTE_PACKET_SIZE) {
							break;
						}
						RemoteFindResultEntry * song = res.add_files();
						song->set_file(qRes->songs[i].fn);
						song->set_file_id(qRes->songs[i].id);
					}
					qRes->free(qRes);
				}
				while (res.ByteSize() > MAX_REMOTE_PACKET_SIZE) {
					res.mutable_files()->RemoveLast();
				}
				string str = res.SerializeAsString();
				REMOTE_HEADER head = { RCMD_FIND_RESULTS, str.length() };
				SendPacket(&head, str.c_str());
			} else {
				sprintf(buf, TEXT("Received malformed @find query: %s"), q.DebugString().c_str());
				SetDlgItemText(hWnd, IDC_RESPONSE, buf);
			}
#if defined(DEBUG)
		} else {
			sprintf(buf, TEXT("Unhandled command code 0x%04X"), br->rHead.scmd);
			SetDlgItemText(hWnd, IDC_RESPONSE, buf);
#endif
		}
		return TRUE;
	}
	return FALSE;
}

WM_HANDLER(OnNotify) {
	NMHDR * nm = (NMHDR *)lParam;
	if (nm->idFrom == IDC_TABS && nm->code == TCN_SELCHANGE) {
		SwitchTab(TabCtrl_GetCurSel(nm->hwndFrom));
	}
	return FALSE;
}

BEGIN_HANDLER_MAP(client_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, OnCommand)
	HANDLER_MAP_ENTRY(WM_NOTIFY, OnNotify)
	HANDLER_MAP_ENTRY(WM_USER, OnUser)
	HANDLER_MAP_ENTRY(WM_TIMER, OnTimer)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, OnInitDialog)
	HANDLER_MAP_ENTRY(WM_CLOSE, OnClose)
END_HANDLER_MAP

INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(client_handlers, hWnd, uMsg, wParam, lParam);
}

bool uflag_have_any_of(uint32 flags, uint32 wanted) {
	return (flags & wanted) ? true:false;
}
void uflag_to_string(uint32 flags, char * buf, size_t bufSize) {
	char *p = buf;
	*p++ = '+';
	char ccur = 'a';
	int icur = 1;
	while (icur <= 0x02000000) {
		if (icur & flags) {
			*p++ = ccur;
		}
		icur <<= 1;
		ccur++;
	}
	ccur = 'A';
	while (icur != 0) {
		if (icur & flags) {
			*p++ = ccur;
		}
		icur <<= 1;
		ccur++;
	}
	*p = 0;
}
