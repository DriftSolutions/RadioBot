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

#include "shell.h"
#include <Strsafe.h>
#include <Shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")

Titus_Buffer line_buffer;
Titus_Sockets socks;

void run_bot();
void stop_bot();
void PrintLog(const char * str);
WM_HANDLER(WndProc);

CONFIG config;

void LoadConfig() {
	Universal_Config * cfg = new Universal_Config();
	if (cfg->LoadConfig(config.fnConf)) {
		DS_CONFIG_SECTION * sec = cfg->GetSection(NULL, "Shell");
		if (sec) {
			DS_VALUE * val = cfg->GetSectionValue(sec, "KeepBotRunning");
			if (val) {
				config.keep_bot_running = val->lLong ? true:false;
			}
			val = cfg->GetSectionValue(sec, "AutoStart");
			if (val) {
				config.start_bot_at_startup = val->lLong ? true:false;
			}
		}
	}
	delete cfg;
}
void SaveConfig() {
	Universal_Config * cfg = new Universal_Config();
	cfg->LoadConfig(config.fnConf);
	DS_CONFIG_SECTION * sec = cfg->FindOrAddSection(NULL, "Shell");
	if (sec) {
		DS_VALUE val;
		memset(&val,0,sizeof(val));
		val.type = DS_TYPE_LONG;
		val.lLong = config.start_bot_at_startup;
		cfg->SetSectionValue(sec, "AutoStart", &val);
		val.lLong = config.keep_bot_running;
		cfg->SetSectionValue(sec, "KeepBotRunning", &val);
	}
	cfg->WriteConfig(config.fnConf);
	delete cfg;
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	memset(&config, 0, sizeof(config));

#if defined(DEBUG)
	//config.keep_bot_running = true;
	//config.start_bot_at_startup = true;
#endif

	char * p = GetUserConfigFile("ShoutIRC", "v5shell.conf");
	strlcpy(config.fnConf, p, sizeof(config.fnConf));
	zfree(p);
	LoadConfig();

	config.hInstance = hInstance;
	config.trayCreated = RegisterWindowMessage(TEXT("TrayCreated"));
	config.port=1024;
	config.lSock = socks.Create();
	while (config.port <= 65535 && !socks.BindToAddr(config.lSock, "127.0.0.1", config.port)) {
		config.port++;
	}
	if (config.port > 65535) {
		MessageBox(NULL, "Error creating control socket!", "Fatal Error", MB_ICONERROR);
		return 2;
	}
	socks.Listen(config.lSock);

	HWND hWnd = config.mWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAINDLG), NULL, WndProc);
	if (hWnd == NULL) {
		MessageBox(NULL, "Error creating window!", "Fatal Error", MB_ICONERROR);
		return 1;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		if (!IsDialogMessage(hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (config.logBG) { DeleteObject(config.logBG); }
	if (config.cSock[0]) { socks.Close(config.cSock[0]); }
	if (config.cSock[1]) { socks.Close(config.cSock[1]); }
	socks.Close(config.lSock);
	SaveConfig();
	return 0;
}

WM_HANDLER(OnSize) {
	RECT rc;
	GetClientRect(hWnd, &rc);
	MoveWindow(GetDlgItem(hWnd, IDC_LOG), 0, 0, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	return TRUE;
}

//#define FONT_NAME "OCR A Extended"
#define FONT_NAME "Lucida Console"
WM_HANDLER(InitDialog) {
	config.mWnd = hWnd;
	HFONT hFont = CreateFont(-MulDiv(8, GetDeviceCaps(GetDC(hWnd), LOGPIXELSY), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, FONT_NAME);
	SendDlgItemMessage(hWnd, IDC_LOG, WM_SETFONT, (WPARAM)hFont, TRUE);
	const HANDLE hbicon = LoadImage(config.hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
	if (hbicon) {
		SendMessageA(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hbicon);
	}
	const HANDLE hsicon = LoadImage(config.hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
	if (hsicon) {
		SendMessageA(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hsicon);
	}

	CheckMenuItem(GetMenu(hWnd), ID_OPTIONS_KEEPRUNNING, MF_BYCOMMAND| config.keep_bot_running ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(GetMenu(hWnd), ID_OPTIONS_AUTOSTART, MF_BYCOMMAND| config.start_bot_at_startup ? MF_CHECKED:MF_UNCHECKED);
	EnableMenuItem(GetMenu(hWnd), ID_CONTROL_RUN, MF_BYCOMMAND|MF_ENABLED);
	EnableMenuItem(GetMenu(hWnd), ID_CONTROL_STOP, MF_BYCOMMAND|MF_GRAYED);
	EnableMenuItem(GetMenu(hWnd), ID_CONTROL_RESTART, MF_BYCOMMAND|MF_GRAYED);

	OnSize(hWnd, WM_SIZE, 0, 0);
	ShowWindow(hWnd, SW_SHOWNORMAL);
	SetTimer(hWnd, 1, 50, NULL);
	AddTrayIcon();

#if defined(DEBUG)
	PrintLog("Test 1");
	PrintLog("Test 2");
	PrintLog("Test 3");
#endif
	if (config.start_bot_at_startup) {
		config.startBotAt = time(NULL)+3;
	}
	return TRUE;
}

WM_HANDLER(OnClose) {
	if (config.proc.bot_running) {
		MessageBox(hWnd, "You can't close the shell when the bot is running!", "Error", MB_ICONERROR);
	} else {
		KillTimer(hWnd, 1);
		DelTrayIcon();
		EndDialog(hWnd, 0);
		PostQuitMessage(0);
	}
	return TRUE;
}

void run_bot() {
	if (config.proc.bot_running) {
		PrintLog("[shell] Bot is already running...");
		return;
	}

	char buf[MAX_PATH],exe[MAX_PATH];
	getcwd(buf, sizeof(buf));
	strcpy(exe, buf);
	if (exe[strlen(exe)-1] != PATH_SEP) {
		strlcat(exe, PATH_SEPS, sizeof(exe));
	}
#if defined(DEBUG)
	strlcat(exe, "RadioBot_d.exe", sizeof(exe));
#else
	strlcat(exe, "RadioBot.exe", sizeof(exe));
#endif
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO si;
	memset(&sa, 0, sizeof(sa));
	memset(&si, 0, sizeof(si));

	sa.bInheritHandle = TRUE;
	sa.nLength = sizeof(sa);

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	CreatePipe(&config.proc.hStdOut, &si.hStdOutput, &sa, 1024);
	//CreatePipe(&config.proc.hStdErr, &si.hStdError, &sa, 1024);

	char parms[MAX_PATH];
	sprintf(parms, "RadioBot.exe -p %d", config.port);
	if (CreateProcess(exe, parms, &sa, NULL, TRUE, NORMAL_PRIORITY_CLASS|CREATE_NEW_PROCESS_GROUP, NULL, buf, &si, &config.proc.pi)) {
		PrintLog("RadioBot process starting...");
		UpdateBalloon("RadioBot v5 Shell", "RadioBot is starting...", 5);
		config.proc.bot_running = true;

		EnableMenuItem(GetMenu(config.mWnd), ID_CONTROL_RUN, MF_BYCOMMAND|MF_GRAYED);
		EnableMenuItem(GetMenu(config.mWnd), ID_CONTROL_STOP, MF_BYCOMMAND|MF_ENABLED);
		EnableMenuItem(GetMenu(config.mWnd), ID_CONTROL_RESTART, MF_BYCOMMAND|MF_ENABLED);
	} else {
		DWORD err =  GetLastError();
		snprintf(exe, sizeof(exe), "[shell] Error executing bot! (Is RadioBot.exe in the same folder as this program?) (errno: %u)", err);
		PrintLog(exe);
		CloseHandle(config.proc.hStdOut);
		CloseHandle(si.hStdOutput);
		UpdateBalloon("RadioBot v5 Shell", "Error starting RadioBot!", 15);
	}
}

void stop_bot(bool manual_stop) {
	if (config.proc.bot_running) {
		if (config.cSock[0] || config.cSock[1]) {
			config.manual_stop = manual_stop;
			PrintLog("[shell] Sending shutdown command...");
			//DWORD bwrote = 0;
			if (config.cSock[0]) {
				socks.Send(config.cSock[0], "exit\n", 5);
			}
			if (config.cSock[1]) {
				socks.Send(config.cSock[1], "exit\n", 5);
			}
		} else {
			PrintLog("[shell] Error: RadioBot is not connected to the control port!");
		}
	}
}

void restart_bot() {
	if (config.proc.bot_running) {
		if (config.cSock[0] || config.cSock[1]) {
			PrintLog("[shell] Sending restart command...");
			//DWORD bwrote = 0;
			if (config.cSock[0]) {
				socks.Send(config.cSock[0], "restart\n", 5);
			}
			if (config.cSock[1]) {
				socks.Send(config.cSock[1], "restart\n", 5);
			}
		} else {
			PrintLog("[shell] Error: RadioBot is not connected to the control port!");
		}
	}
}

void on_bot_closed() {
	if (config.cSock[0]) { socks.Close(config.cSock[0]); config.cSock[0] = NULL; }
	if (config.cSock[1]) { socks.Close(config.cSock[1]); config.cSock[1] = NULL; }
	CloseHandle(config.proc.pi.hThread);
	CloseHandle(config.proc.pi.hProcess);
	CloseHandle(config.proc.hStdOut);
	memset(&config.proc, 0, sizeof(config.proc));
	PrintLog("[shell] RadioBot has exited.");
	UpdateBalloon("RadioBot v5 Shell", "RadioBot has exited.", 5);

	EnableMenuItem(GetMenu(config.mWnd), ID_CONTROL_RUN, MF_BYCOMMAND|MF_ENABLED);
	EnableMenuItem(GetMenu(config.mWnd), ID_CONTROL_STOP, MF_BYCOMMAND|MF_GRAYED);
	EnableMenuItem(GetMenu(config.mWnd), ID_CONTROL_RESTART, MF_BYCOMMAND|MF_GRAYED);

	if (!config.manual_stop && config.keep_bot_running) {
		config.startBotAt = time(NULL)+5;
	}
	config.manual_stop = false;
}

void PrintLog(const char * str) {
	HWND lWnd = GetDlgItem(config.mWnd, IDC_LOG);
	while (ListBox_GetCount(lWnd) > 500) {
		ListBox_DeleteString(lWnd, 0);
	}
	int n = ListBox_AddString(lWnd, str);
	if (n != -1) {
		ListBox_SetCurSel(lWnd, n);
	}
}

void proc_buffer() {
	char *buf = line_buffer.Get();
	char *p = buf;
	for (uint32 i=0; i < line_buffer.GetLen(); i++) {
		if (*p == '\n') {
			*p = 0;
			PrintLog(line_buffer.Get());
			line_buffer.RemoveFromBeginning(i+1);
			p = buf = line_buffer.Get();
			i = -1;
		} else {
			p++;
		}
	}
}

inline void handle_sock(int num) {
	char buf=0;
	if (socks.Recv(config.cSock[num], &buf, 1) <= 0) {
		PrintLog("[shell] RadioBot closed control connection");
		socks.Close(config.cSock[num]);
		config.cSock[num] = NULL;
	}
}

void handle_sockets() {
	TITUS_SOCKET_LIST tfd;
	TFD_ZERO(&tfd);
	TFD_SET(&tfd, config.lSock);
	if (config.cSock[0]) { TFD_SET(&tfd, config.cSock[0]); }
	if (config.cSock[1]) { TFD_SET(&tfd, config.cSock[1]); }
	if (socks.Select_Read_List(&tfd, uint32(0)) > 0) {
		if (TFD_ISSET(&tfd, config.lSock)) {
			if (config.cSock[0] == NULL) {
				config.cSock[0] = socks.Accept(config.lSock);
				PrintLog("[shell] Control pipe connected.");
			} else if (config.cSock[1] == NULL) {
				config.cSock[1] = socks.Accept(config.lSock);
				PrintLog("[shell] Control pipe connected.");
			} else {
				PrintLog("[shell] Client trying to connect, but already have 2 connections...");
			}
		}
		if (TFD_ISSET(&tfd, config.cSock[0])) {
			handle_sock(0);
		}
		if (TFD_ISSET(&tfd, config.cSock[1])) {
			handle_sock(1);
		}
	}
}

WM_HANDLER(OnTimer) {
	if (wParam == 1) {
		if (IsIconic(config.mWnd)) {
			ShowWindow(config.mWnd, SW_HIDE);
		}
		handle_sockets();

		if (config.proc.bot_running) {
			DWORD avail = 0;
			if (PeekNamedPipe(config.proc.hStdOut, NULL, 0, NULL, &avail, NULL) && avail > 0) {
				DWORD bread=0;
				char buf[1024];
				if (ReadFile(config.proc.hStdOut, buf, avail < sizeof(buf) ? avail:sizeof(buf), &bread, NULL) && bread > 0) {
					for (DWORD i=0; i < bread; i++) {
						if (buf[i] == 0) { buf[i] = ' '; }
					}
					line_buffer.Append(buf, bread);
					proc_buffer();
				}
			}

			if (WaitForSingleObject(config.proc.pi.hProcess, 0) == WAIT_OBJECT_0) {
				on_bot_closed();
			}
		} else {
			if (config.startBotAt != 0 && config.startBotAt <= time(NULL)) {
				config.startBotAt = 0;
				run_bot();
			}
		}

		return TRUE;
	}
	return FALSE;
}

WM_HANDLER(OnCommand) {
	switch (LOWORD(wParam)) {
		case ID_CONTROL_RUN:
			run_bot();
			break;
		case ID_CONTROL_STOP:
			stop_bot(true);
			break;
		case ID_CONTROL_RESTART:
			restart_bot();
			break;
		case ID_CONTROL_EXIT:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case ID_OPTIONS_KEEPRUNNING:
			config.keep_bot_running = config.keep_bot_running ? false:true;
			CheckMenuItem(GetMenu(hWnd), ID_OPTIONS_KEEPRUNNING, MF_BYCOMMAND| config.keep_bot_running ? MF_CHECKED:MF_UNCHECKED);
			SaveConfig();
			break;
		case ID_OPTIONS_AUTOSTART:
			config.start_bot_at_startup = config.start_bot_at_startup ? false:true;
			CheckMenuItem(GetMenu(hWnd), ID_OPTIONS_AUTOSTART, MF_BYCOMMAND| config.start_bot_at_startup ? MF_CHECKED:MF_UNCHECKED);
			SaveConfig();
			break;

		case ID_HELP_ABOUT:
			MessageBox(hWnd, "RadioBot v5 Shell 1.0\r\nCompiled at " __TIME__ " on " __DATE__ ".\r\n\r\nCopyright 2012-2017 ShoutIRC.com. All rights reserved.", "About...", MB_ICONINFORMATION);
			break;

		case ID_HELP_PACKAGEMANAGER:
			char buf[MAX_PATH]={0};
			getcwd(buf, sizeof(buf));
			if ((int)ShellExecute(hWnd, "open", "PackageManager.exe", NULL, buf, SW_SHOWNORMAL) > 32) {
				if (!config.proc.bot_running) {
					SendMessage(hWnd, WM_CLOSE, 0, 0);
				} else {
					MessageBox(hWnd, "Remember to close the RadioBot Shell before performing an update or Package Manager won't be able to update it.", "Notice", MB_ICONINFORMATION);
				}
			} else {
				MessageBox(hWnd, "Error executing PackageManager.exe!", "Error", MB_ICONERROR);
			}

			break;
	}
	return FALSE;
}

WM_HANDLER(TrayHandler) {
	if (lParam == WM_RBUTTONUP) {
	} else if (lParam == WM_LBUTTONDBLCLK) {
		if (IsIconic(hWnd)) {
			ShowWindow(hWnd, SW_SHOW);
			ShowWindow(hWnd, SW_RESTORE);
		} else {
			ShowWindow(hWnd, SW_MINIMIZE);
			ShowWindow(hWnd, SW_HIDE);
		}
		/*
		if (GetWindowLong(config.mWnd, GWL_STYLE) & WS_VISIBLE) {
			ShowWindow(config.mWnd, SW_HIDE);
		} else {
			ShowWindow(config.mWnd, SW_NORMAL);
		}
		*/
	}
	return FALSE;
}

WM_HANDLER(OnLogColor) {
	if ((HWND)lParam == GetDlgItem(config.mWnd, IDC_LOG)) {
		SetDCPenColor((HDC)wParam, RGB(255,255,255));
		if (config.logBG == NULL) {
			config.logBG = CreateSolidBrush(RGB(0,0,0));
		}
		return (INT_PTR)config.logBG;
	}
	return FALSE;
}

WM_HANDLER(OnDrawItem) {
	DRAWITEMSTRUCT * pdis = (DRAWITEMSTRUCT *)lParam;

	// If there are no list box items, skip this message.
	if (pdis->itemID == -1) {
		return TRUE;
	}

	if (pdis->CtlID != IDC_LOG) {
		return FALSE;
	}

	char achBuffer[1024];
	TEXTMETRIC tm;

	if (pdis->itemAction == ODA_SELECT || pdis->itemAction == ODA_DRAWENTIRE) {
		// Draw the string associated with the item.
		// Get the item string from the list box.
		if (SendMessage(pdis->hwndItem, LB_GETTEXTLEN, pdis->itemID, 0) < sizeof(achBuffer)) {
			SendMessage(pdis->hwndItem, LB_GETTEXT, pdis->itemID, (LPARAM)achBuffer);
		} else {
			strlcpy(achBuffer, "Line too long!", sizeof(achBuffer));
		}
		// Get the metrics for the current font.
		GetTextMetrics(pdis->hDC, &tm);
		// Calculate the vertical position for the item string
		// so that the string will be vertically centered in the
		// item rectangle.
		int yPos = (pdis->rcItem.bottom + pdis->rcItem.top - tm.tmHeight) / 2;

		// Get the character length of the item string.
		size_t cch=0;
		HRESULT hr = StringCchLength(achBuffer, sizeof(achBuffer), &cch);
		if (FAILED(hr))
		{
			// TODO: Handle error.
		}

		// Draw the string in the item rectangle, leaving a six
		// pixel gap between the item bitmap and the string.
		SetBkMode(pdis->hDC, TRANSPARENT);
		SetTextColor(pdis->hDC, RGB(0xC0, 0xC0, 0xC0));
		TextOut(pdis->hDC, 2, yPos, achBuffer, cch);

		// Clean up.
		//SelectObject(hdcMem, hbmpOld);
		//DeleteDC(hdcMem);

		// Is the item selected?
		/*
		if (pdis->itemState & ODS_SELECTED)
		{
			// Set RECT coordinates to surround only the
			// bitmap.
			rcBitmap.left = pdis->rcItem.left;
			rcBitmap.top = pdis->rcItem.top;
			rcBitmap.right = pdis->rcItem.left + XBITMAP;
			rcBitmap.bottom = pdis->rcItem.top + YBITMAP;

			// Draw a rectangle around bitmap to indicate
			// the selection.
			DrawFocusRect(pdis->hDC, &rcBitmap);
		}
		*/
	}
	return TRUE;
}

WM_HANDLER(OnSysCommand) {
	if (wParam == SC_MINIMIZE) {
		//ShowWindow(hWnd, SW_HIDE);
	}
	/*
	else if (wParam == SC_RESTORE) {
		ShowWindow(hWnd, SW_SHOW);
	}
	*/
	return FALSE;
}

BEGIN_HANDLER_MAP(ShellMap)
	HANDLER_MAP_ENTRY(WM_TIMER, OnTimer)
	HANDLER_MAP_ENTRY(WM_SIZE, OnSize)
	HANDLER_MAP_ENTRY(WM_DRAWITEM, OnDrawItem)
	HANDLER_MAP_ENTRY(WM_COMMAND, OnCommand)
	HANDLER_MAP_ENTRY(WM_USER+1, TrayHandler)
	HANDLER_MAP_ENTRY(WM_CTLCOLORLISTBOX, OnLogColor)
	HANDLER_MAP_ENTRY(WM_SYSCOMMAND, OnSysCommand)
	HANDLER_MAP_ENTRY(WM_CLOSE, OnClose)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, InitDialog)
END_HANDLER_MAP

WM_HANDLER(WndProc) {
	if (uMsg == config.trayCreated && config.trayCreated != 0) {
		DelTrayIcon();
		AddTrayIcon();
	}
	HANDLE_MAP(ShellMap);
}
