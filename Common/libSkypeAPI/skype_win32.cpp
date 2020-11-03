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

/*** libSkypeAPI: Skype API Library ***

Written by Indy Sams (indy@driftsolutions.com)
Loosely based on the Skype Win32 Demo Code

This file is part of the libSkypeAPI library, for licensing see the LICENSE file that came with this package.

***/

#if defined(WIN32)
#include "skype.h"

unsigned int __stdcall SkypeAPI_WorkThread(void *);
bool SkypeAPI_CreateWindowClass();
bool SkypeAPI_CreateWindow();
void SkypeAPI_DestroyWindowClass();
void SkypeAPI_DestroyWindow();

bool skype_win32_connect() {
	h.MsgAttach   = RegisterWindowMessage("SkypeControlAPIAttach");
	h.MsgDiscover = RegisterWindowMessage("SkypeControlAPIDiscover");

	if (h.MsgAttach != 0 && h.MsgDiscover != 0) {
		if (SkypeAPI_CreateWindowClass()) {
			h.hThread = (HANDLE)_beginthreadex(NULL, 64*1024, SkypeAPI_WorkThread, NULL, 0, &h.dwThreadID);
			if (h.hThread != (HANDLE)0) {
				return true;
			} else {
				SkypeAPI_DestroyWindowClass();
				return false;
			}
		} else {
			return false;
		}
	} else {
		return false;
	}
}

bool skype_win32_send_command(char * str) {
	COPYDATASTRUCT oCopyData;

	// send command to skype
	oCopyData.dwData = 0;
	oCopyData.lpData = str;
	oCopyData.cbData = strlen(str)+1;
	if (oCopyData.cbData != 1) {
		if (SendMessageTimeout(h.SkypeWnd, WM_COPYDATA, (WPARAM)h.hWnd, (LPARAM)&oCopyData, SMTO_ABORTIFHUNG, 1000, NULL) == FALSE) {
			return false;
		}
	}
	return true;
}

void skype_win32_disconnect() {
	SendMessage(h.hWnd, WM_CLOSE, 0, 0);
	WaitForSingleObject(h.hThread, INFINITE);
	CloseHandle(h.hThread);
	SkypeAPI_DestroyWindowClass();
}

unsigned int __stdcall SkypeAPI_WorkThread(void *) {
	if (SkypeAPI_CreateWindow()) {
		h.client->status_cb(SAPI_STATUS_CONNECTING);
		if (SendMessageTimeout(HWND_BROADCAST, h.MsgDiscover, (WPARAM)h.hWnd, 0, SMTO_ABORTIFHUNG, 1000, NULL) != 0) {
			MSG msg;
			while(!h.quit && GetMessage(&msg, 0, 0, 0) != 0) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			h.client->status_cb(SAPI_STATUS_DISCONNECTED);
		} else {
			h.client->status_cb(SAPI_STATUS_ERROR);
		}
		SkypeAPI_DestroyWindow();
	} else {
		h.client->status_cb(SAPI_STATUS_ERROR);
	}

	_endthreadex(0);
    return 0;
}

LRESULT CALLBACK SkypeAPI_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	switch (uMsg) {
		case WM_CLOSE:
			PostThreadMessage(h.dwThreadID, WM_QUIT, 0, 0);
			break;
		case WM_COPYDATA:
			if (h.SkypeWnd == (HWND)wParam) {
				PCOPYDATASTRUCT poCopyData=(PCOPYDATASTRUCT)lParam;
				h.client->message_cb((char *)poCopyData->lpData);
				return 1;
			}
			break;
		default:
			if (uMsg == h.MsgAttach) {
				switch(lParam)	{
					case SKYPECONTROLAPI_ATTACH_SUCCESS:
						h.SkypeWnd=(HWND)wParam;
						char buf[32];
						sprintf(buf, "PROTOCOL %d", h.client->protocol_version);
						SkypeAPI_SendCommand(buf);
						h.client->status_cb(SAPI_STATUS_CONNECTED);
						break;
					case SKYPECONTROLAPI_ATTACH_PENDING_AUTHORIZATION:
						h.client->status_cb(SAPI_STATUS_PENDING_AUTHORIZATION);
						break;
					case SKYPECONTROLAPI_ATTACH_API_AVAILABLE:
						h.client->status_cb(SAPI_STATUS_AVAILABLE);
						break;
					case SKYPECONTROLAPI_ATTACH_NOT_AVAILABLE:
						h.client->status_cb(SAPI_STATUS_NOT_AVAILABLE);
						break;
					case SKYPECONTROLAPI_ATTACH_REFUSED:
						h.client->status_cb(SAPI_STATUS_REFUSED);
						break;
				}
				return 1;
			}
			break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam);
}

bool SkypeAPI_CreateWindowClass() {
	WNDCLASS wc;

	_snprintf(h.ClassName, sizeof(h.ClassName)-1, "libSkypeAPI-Communication-Window-%u-%u", GetCurrentProcessId(), GetCurrentThreadId());
	wc.style=CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS;
	wc.lpfnWndProc=SkypeAPI_WndProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=h.client->hInstance;
	wc.hIcon=NULL;
	wc.hCursor=NULL;
	wc.hbrBackground=NULL;
	wc.lpszMenuName=NULL;
	wc.lpszClassName=h.ClassName;

	return RegisterClass(&wc) ? true:false;
}

void SkypeAPI_DestroyWindowClass() {
	UnregisterClass(h.ClassName, h.client->hInstance);
}

bool SkypeAPI_CreateWindow() {
	h.hWnd = CreateWindowEx( WS_EX_APPWINDOW|WS_EX_WINDOWEDGE, h.ClassName, "", WS_BORDER|WS_SYSMENU|WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 128, 128, NULL, 0, h.client->hInstance, 0);
	return (h.hWnd != NULL) ? true:false;
}

void SkypeAPI_DestroyWindow() {
	DestroyWindow(h.hWnd);
}

#endif
