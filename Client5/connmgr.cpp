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

#include "client.h"

INT_PTR CALLBACK ConnMgrProc(HWND, UINT, WPARAM, LPARAM);
int curhost = -1;

bool OpenConnManager(HWND hParent) {
	DialogBox(config.hInst, MAKEINTRESOURCE(IDD_CONNMGR), hParent, ConnMgrProc);
	return true;
}

void SaveConnInfo(HWND hWnd) {
	if (curhost != -1) {
		int i = curhost;
		GetDlgItemText(hWnd, IDC_CM_CONNAME, config.hosts[i].name, sizeof(config.hosts[i].name));
		GetDlgItemText(hWnd, IDC_CM_HOST, config.hosts[i].host, sizeof(config.hosts[i].host));
		GetDlgItemText(hWnd, IDC_CM_USER, config.hosts[i].username, sizeof(config.hosts[i].username));
		GetDlgItemText(hWnd, IDC_CM_PASS, config.hosts[i].pass, sizeof(config.hosts[i].pass));
		config.hosts[i].port = SendDlgItemMessage(hWnd, IDC_CM_SPIN, UDM_GETPOS32, 0, 0);
		if (IsDlgButtonChecked(hWnd, IDC_CM_AUTO)) {
			config.hosts[i].mode = 0;
		} else if (IsDlgButtonChecked(hWnd, IDC_CM_SSL)) {
			config.hosts[i].mode = 1;
		} else {
			config.hosts[i].mode = 2;
		}
	}
}

int ctls[] = { IDC_CM_CONNAME, IDC_CM_HOST, IDC_CM_USER, IDC_CM_PASS, IDC_CM_PORT, IDC_CM_AUTO, IDC_CM_SSL, IDC_CM_PLAIN, IDC_CM_SAVE, -1 };
void DisableCtls(HWND hWnd) {
	for (int j=0; ctls[j] != -1; j++) {
		EnableWindow(GetDlgItem(hWnd, ctls[j]), FALSE);
	}
}

void LoadConnInfo(HWND hWnd, int i) {
	for (int j=0; ctls[j] != -1; j++) {
		EnableWindow(GetDlgItem(hWnd, ctls[j]), TRUE);
	}

	curhost = i;
	SetDlgItemText(hWnd, IDC_CM_CONNAME, config.hosts[i].name);
	SetDlgItemText(hWnd, IDC_CM_HOST, config.hosts[i].host);
	SetDlgItemText(hWnd, IDC_CM_USER, config.hosts[i].username);
	SetDlgItemText(hWnd, IDC_CM_PASS, config.hosts[i].pass);
	SendDlgItemMessage(hWnd, IDC_CM_SPIN, UDM_SETPOS32, 0, config.hosts[i].port);
	if (config.hosts[i].mode == 0) {
		CheckRadioButton(hWnd, IDC_CM_AUTO, IDC_CM_PLAIN, IDC_CM_AUTO);
	} else if (config.hosts[i].mode == 1) {
		CheckRadioButton(hWnd, IDC_CM_AUTO, IDC_CM_PLAIN, IDC_CM_SSL);
	} else {
		CheckRadioButton(hWnd, IDC_CM_AUTO, IDC_CM_PLAIN, IDC_CM_PLAIN);
	}
}

void AddToolHelp(HWND hTip, HWND hParent, int id, TCHAR * text) {
	TOOLINFO ti;
	memset(&ti, 0, sizeof(ti));
	ti.cbSize = sizeof(ti);
	ti.hwnd = hParent;
	ti.uId = (UINT_PTR)GetDlgItem(hParent, id);
	ti.uFlags = TTF_IDISHWND|TTF_SUBCLASS;
	ti.lpszText = text;
	SendMessage(hTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}
HWND CreateToolHelp(HWND hParent) {
	HWND hTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_VISIBLE|WS_POPUP|TTS_ALWAYSTIP|TTS_BALLOON|TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hParent, NULL, config.hInst, NULL);
	SendMessage(hTip, TTM_SETMAXTIPWIDTH, 0, 500);
	return hTip;
}

WM_HANDLER(ConnMgr_OnInitDialog) {
	HWND hTip = CreateToolHelp(hWnd);//CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL, WS_VISIBLE|WS_POPUP|TTS_ALWAYSTIP|TTS_BALLOON|TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWnd, NULL, config.hInst, NULL);
	AddToolHelp(hTip, hWnd, IDC_CM_HOST, TEXT("This is the hostname or IP of the PC the bot is running on.\r\nIt is not your IRC or web server hostname/IP."));
	AddToolHelp(hTip, hWnd, IDC_CM_USER, TEXT("This is your username in the bot.\r\nIt is usually the same as your IRC nickname, PM the bot !whoami to verify."));
	AddToolHelp(hTip, hWnd, IDC_CM_PASS, TEXT("This is your password in the bot.\r\nYou can set/change your password by PMing the bot !chpass"));
	AddToolHelp(hTip, hWnd, IDC_CM_PORT, TEXT("This is the RadioBot remote connection port, usually 10000 or 10001.\r\nThis is not your IRC or web server port."));
	AddToolHelp(hTip, hWnd, IDC_CM_AUTO, TEXT("Will attempt to connect with SSL, but will fallback to plain if it fails. (Recommended Setting)"));

	SendDlgItemMessage(hWnd, IDC_CM_SPIN, UDM_SETRANGE32, 1, 65535);
	SendDlgItemMessage(hWnd, IDC_CM_SPIN, UDM_SETPOS32, 0, 10000);
	for (int i=0; i < MAX_CONNECTIONS; i++) {
		if (config.hosts[i].name[0]) {
			ListBox_AddString(GetDlgItem(hWnd, IDC_CM_LIST), config.hosts[i].name);
		} else {
			break;
		}
	}
	int num = ListBox_GetCount(GetDlgItem(hWnd, IDC_CM_LIST));
	if (num > 0) {
		hostMutex.Lock();
		if (config.host_index >= num) {
			config.host_index = 0;
		}
		ListBox_SetCurSel(GetDlgItem(hWnd, IDC_CM_LIST), config.host_index);
		LoadConnInfo(hWnd, config.host_index);
		hostMutex.Release();
	}
	ShowWindow(hWnd, SW_SHOW);
	return TRUE;
}

WM_HANDLER(ConnMgr_OnCommand) {
	if (LOWORD(wParam) == IDC_CM_EXIT) {
		PostMessage(hWnd, WM_CLOSE, 0, 0);
		return TRUE;
	} else if (LOWORD(wParam) == IDC_CM_LIST && HIWORD(wParam) == LBN_SELCHANGE) {
		int n = ListBox_GetCurSel(GetDlgItem(hWnd, IDC_CM_LIST));
		if (n != -1) {
			LoadConnInfo(hWnd, n);
		}
	} else if (LOWORD(wParam) == IDC_CM_CONNECT || (LOWORD(wParam) == IDC_CM_LIST && HIWORD(wParam) == LBN_DBLCLK)) {
		int n = ListBox_GetCurSel(GetDlgItem(hWnd, IDC_CM_LIST));
		if (n != -1) {
			if (!strlen(config.hosts[n].name) || !strlen(config.hosts[n].host) || !strlen(config.hosts[n].username) || !strlen(config.hosts[n].pass)) {
				MessageBox(config.mWnd, TEXT("This connection does not have all required fields set!"), TEXT("Error"), MB_ICONERROR);
			} else {
				EndDialog(hWnd, 0);
				DoConnect(n);
			}
		} else {
			MessageBox(hWnd, TEXT("No connection is currently selected!"), TEXT("Error"), MB_ICONERROR);
		}
	} else if (LOWORD(wParam) == IDC_CM_SAVE) {
		if (curhost != -1) {
			SaveConnInfo(hWnd);
		} else {
			MessageBox(hWnd, TEXT("No connection is currently selected!"), TEXT("Error"), MB_ICONERROR);
		}
	} else if (LOWORD(wParam) == IDC_CM_NEW) {
		for (int i=0; i < MAX_CONNECTIONS; i++) {
			if (config.hosts[i].name[0]) {
				continue;
			}

			sstrcpy(config.hosts[i].name, "Unnamed Connection");
			sstrcpy(config.hosts[i].host, "localhost");
			config.hosts[i].port = 10000;

			ListBox_AddString(GetDlgItem(hWnd, IDC_CM_LIST), config.hosts[i].name);
			ListBox_SetCurSel(GetDlgItem(hWnd, IDC_CM_LIST), i);
			LoadConnInfo(hWnd, i);
			break;
		}
	} else if (LOWORD(wParam) == IDC_CM_DELETE) {
		int n = ListBox_GetCurSel(GetDlgItem(hWnd, IDC_CM_LIST));
		if (n != -1) {
			if (IsConnected()) {
				MessageBox(hWnd, TEXT("This will disconnect your current session..."), TEXT("Notice"), MB_ICONINFORMATION);
				AutoMutex(hostMutex);
				config.connect = false;
			}
			if (n == 0) {
				memmove(&config.hosts[0],&config.hosts[1],sizeof(CONFIG_HOSTS) * (MAX_CONNECTIONS-1));
				memset(&config.hosts[MAX_CONNECTIONS-1], 0, sizeof(CONFIG_HOSTS));
			} else if (n == (MAX_CONNECTIONS-1)) {
				memset(&config.hosts[MAX_CONNECTIONS-1], 0, sizeof(CONFIG_HOSTS));
			} else {
				int num = MAX_CONNECTIONS - curhost - 1;
				memmove(&config.hosts[curhost], &config.hosts[curhost+1], sizeof(config.hosts[0])*num);
				memset(&config.hosts[(MAX_CONNECTIONS-1)], 0, sizeof(config.hosts[(MAX_CONNECTIONS-1)]));
			}

			ListBox_DeleteString(GetDlgItem(hWnd, IDC_CM_LIST), n);
			if (ListBox_GetCount(GetDlgItem(hWnd, IDC_CM_LIST)) > 0) {
				ListBox_SetCurSel(GetDlgItem(hWnd, IDC_CM_LIST), 0);
				LoadConnInfo(hWnd, 0);
			} else {
				curhost = -1;
				DisableCtls(hWnd);
			}
		} else {
			MessageBox(hWnd, TEXT("No connection is currently selected!"), TEXT("Error"), MB_ICONERROR);
		}
	}

	return FALSE;
}

WM_HANDLER(ConnMgr_OnNotify) {
	return FALSE;
}

WM_HANDLER(ConnMgr_OnClose) {
	EndDialog(hWnd, 0);
	return TRUE;
}

BEGIN_HANDLER_MAP(connmgr_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, ConnMgr_OnCommand)
	HANDLER_MAP_ENTRY(WM_NOTIFY, ConnMgr_OnNotify)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, ConnMgr_OnInitDialog)
	HANDLER_MAP_ENTRY(WM_CLOSE, ConnMgr_OnClose)
END_HANDLER_MAP

INT_PTR CALLBACK ConnMgrProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(connmgr_handlers, hWnd, uMsg, wParam, lParam);
}
