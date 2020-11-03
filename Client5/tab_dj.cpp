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
#include "Shlwapi.h"
#pragma comment(lib, "Shlwapi.lib")

INT_PTR CALLBACK TabDJProc(HWND, UINT, WPARAM, LPARAM);

HBRUSH hLoginBrush = NULL;
HBRUSH hLogoutBrush = NULL;
HBRUSH hCountBrush = NULL;

HWND CreateTabDJ(HWND hParent) {
	HWND hWnd = CreateDialog(config.hInst, MAKEINTRESOURCE(IDD_DJ), hParent, TabDJProc);
	return hWnd;
}

WM_HANDLER(DJ_OnInitDialog) {
	hLoginBrush = CreateSolidBrush(RGB(0, 0x80, 0));
	hLogoutBrush = CreateSolidBrush(RGB(0xFF, 0, 0));
	hCountBrush = CreateSolidBrush(RGB(0, 128, 255));

	HWND hTip = CreateToolHelp(hWnd);
	if (hTip) {
		AddToolHelp(hTip, hWnd, IDC_DJ_LOGIN, "Click here to mark yourself as the Live DJ and take requests (without enabling @find)...\r\nThis function requires the flag: +d");
		AddToolHelp(hTip, hWnd, IDC_DJ_LOGIN2, "Click here to mark yourself as the Live DJ and take requests with @find support...\r\nThis function requires the flag: +d and the @find tab configured.");
		AddToolHelp(hTip, hWnd, IDC_SRC_COUNT, "Tell the source plugin to count you in (same as !autodj-stop)\r\nThis function requires the flag: +s");
		//AddToolHelp(hTip, hWnd, IDC_SRC_GRP, "Source plugins play music when there is no live DJ on. Examples include AutoDJ, SimpleDJ, and SAM.");
	}

	return TRUE;
}

WM_HANDLER(DJ_OnCommand) {
	if (IsConnected()) {
		char buf[512];
		if (LOWORD(wParam) == IDC_DJ_LOGIN) {
			SendPacket(RCMD_REQ_LOGIN);
		} else if (LOWORD(wParam) == IDC_DJ_LOGIN2) {
			char opts = 0x01;
			SendPacket(RCMD_REQ_LOGIN, 1, &opts);
		} else if (LOWORD(wParam) == IDC_DJ_LOGOUT) {
			SendPacket(RCMD_REQ_LOGOUT);
		} else if (LOWORD(wParam) == IDC_DJ_REQ_DED) {
			if (InputBox(hWnd, "What text do you want to display for this request?\n(Preview: Current song requested by Your Text)", buf, sizeof(buf))) {
				REMOTE_HEADER head = { RCMD_SEND_REQ, strlen(buf)+1 };
				SendPacket(&head, buf);
			}
		} else if (LOWORD(wParam) == IDC_DJ_DED_TO) {
			if (InputBox(hWnd, "What text do you want to display for this dedication?\n(Preview: Current song is dedicated to: Your Text)", buf, sizeof(buf))) {
				REMOTE_HEADER head = { RCMD_SEND_DED, strlen(buf)+1 };
				SendPacket(&head, buf);
			}
		} else if (LOWORD(wParam) == IDC_DJ_REQUEST) {
			if (InputBox(hWnd, "What song do you want to request?", buf, sizeof(buf))) {
				REMOTE_HEADER head = { RCMD_REQ, strlen(buf)+1 };
				SendPacket(&head, buf);
			}
		} else if (LOWORD(wParam) == IDC_SRC_COUNT) {
			REMOTE_HEADER head = { RCMD_SRC_COUNTDOWN, 0 };
			SendPacket(&head, NULL);
		} else if (LOWORD(wParam) == IDC_SRC_STOP) {
			REMOTE_HEADER head = { RCMD_SRC_FORCE_OFF, 0 };
			SendPacket(&head, NULL);
		} else if (LOWORD(wParam) == IDC_SRC_START) {
			REMOTE_HEADER head = { RCMD_SRC_FORCE_ON, 0 };
			SendPacket(&head, NULL);
		} else if (LOWORD(wParam) == IDC_SRC_RELOAD) {
			REMOTE_HEADER head = { RCMD_SRC_RELOAD, 0 };
			SendPacket(&head, NULL);
		} else if (LOWORD(wParam) == IDC_SRC_NEXT) {
			REMOTE_HEADER head = { RCMD_SRC_NEXT, 0 };
			SendPacket(&head, NULL);
			/*
		} else if (LOWORD(wParam) == IDC_SRC_STATUS) {
			REMOTE_HEADER head = { RCMD_SRC_STATUS, 0 };
			SendPacket(&head, NULL);
		} else if (LOWORD(wParam) == IDC_SRC_NAME) {
			REMOTE_HEADER head = { RCMD_SRC_GET_NAME, 0 };
			SendPacket(&head, NULL);
			*/
		}
	}

	if (LOWORD(wParam) == IDC_DJ_CLEAR) {
		ListBox_ResetContent(GetDlgItem(hWnd, IDC_DJ_REQS));
	} else if (LOWORD(wParam) == IDC_DJ_REQS && HIWORD(wParam) == LBN_DBLCLK) {
		int sel = ListBox_GetCurSel(GetDlgItem(hWnd, IDC_DJ_REQS));
		if (sel >= 0) {
			if ((sel%2) != 0) { sel--; }
			ListBox_DeleteString(GetDlgItem(hWnd, IDC_DJ_REQS), sel);
			ListBox_DeleteString(GetDlgItem(hWnd, IDC_DJ_REQS), sel);
		}
	}

	return FALSE;
}

WM_HANDLER(DJ_OnColorButton) {
	if ((HWND)lParam == GetDlgItem(hWnd, IDC_DJ_LOGIN) || (HWND)lParam == GetDlgItem(hWnd, IDC_DJ_LOGIN2)) {
		return (INT_PTR)hLoginBrush;
	}
	if ((HWND)lParam == GetDlgItem(hWnd, IDC_DJ_LOGOUT)) {
		return (INT_PTR)hLogoutBrush;
	}
	if ((HWND)lParam == GetDlgItem(hWnd, IDC_SRC_COUNT)) {
		return (INT_PTR)hCountBrush;
	}
	return FALSE;
}

WM_HANDLER(DJ_OnDestroy) {
	if (hLoginBrush) {
		DeleteObject(hLoginBrush);
		hLoginBrush = NULL;
	}
	if (hLogoutBrush) {
		DeleteObject(hLogoutBrush);
		hLoginBrush = NULL;
	}
	if (hCountBrush) {
		DeleteObject(hCountBrush);
		hCountBrush = NULL;
	}
	return TRUE;
}

WM_HANDLER(DJ_ContextMenu) {
	if ((HWND)wParam == GetDlgItem(hWnd, IDC_DJ_REQS)) {
		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		int ind = LBItemFromPt(GetDlgItem(hWnd, IDC_DJ_REQS), pt, FALSE);
		if (ind != -1) {
			int len = ListBox_GetTextLen(GetDlgItem(hWnd, IDC_DJ_REQS), ind);
			char * str = (char *)malloc(len+1);
			ListBox_GetText(GetDlgItem(hWnd, IDC_DJ_REQS), ind, str);
			bool suc = false;
			if (OpenClipboard(hWnd)) {
				EmptyClipboard();
				HGLOBAL hglob = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE | GMEM_ZEROINIT,  len+1);
				if (hglob) {
					void *pvGlob = GlobalLock(hglob);
					HANDLE hRc = NULL;
					if (pvGlob) {
						CopyMemory(pvGlob, str, len+1);
						GlobalUnlock(hglob);
						hRc = SetClipboardData(CF_OEMTEXT, hglob);
						suc = (hRc == NULL) ? false:true;
					}
					if (!hRc) {
						GlobalFree(hglob);
					}
				}
				CloseClipboard();
			}
			free(str);
			if (suc) {
				SHMessageBoxCheck(hWnd, "Text copied to clipboard.", "Clipboard", MB_OK|MB_ICONINFORMATION, IDOK, "shoutirc_{8D877D81-F6C0-443D-97A0-0122A5AC3D5D}");
			} else {
				MessageBox(hWnd, "Error copying text to clipboard.", "Clipboard", MB_ICONERROR);
			}
		}
	}
	return FALSE;
}


BEGIN_HANDLER_MAP(tab_dj_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, DJ_OnCommand)
	HANDLER_MAP_ENTRY(WM_CONTEXTMENU, DJ_ContextMenu)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, DJ_OnInitDialog)
	HANDLER_MAP_ENTRY(WM_CTLCOLORBTN, DJ_OnColorButton)
	HANDLER_MAP_ENTRY(WM_DESTROY, DJ_OnDestroy)
END_HANDLER_MAP

INT_PTR CALLBACK TabDJProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(tab_dj_handlers, hWnd, uMsg, wParam, lParam);
}
