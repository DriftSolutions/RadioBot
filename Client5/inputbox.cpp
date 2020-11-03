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

struct INPUTBOXPARAMS {
	char * query;
	char * buf;
	size_t bufSize;
};

WM_HANDLER(InputBox_OnInitDialog) {
	INPUTBOXPARAMS * ibp = (INPUTBOXPARAMS *)lParam;
	SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
	SetDlgItemText(hWnd, IDC_QUERY, ibp->query);
	PostMessage(hWnd, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hWnd, IDC_RESP), TRUE);
	return TRUE;
}

WM_HANDLER(InputBox_OnCommand) {
	if (LOWORD(wParam) == IDCANCEL) {
		EndDialog(hWnd, 0);
		return TRUE;
	} else if (LOWORD(wParam) == IDOK) {
		INPUTBOXPARAMS * ibp = (INPUTBOXPARAMS *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (ibp != NULL) {
			int n = GetDlgItemText(hWnd, IDC_RESP, ibp->buf, ibp->bufSize);
			EndDialog(hWnd, n > 0 ? 1:0);
		} else {
			MessageBox(hWnd, "Error getting INPUTBOXPARAMS!", "Error", MB_ICONERROR);
			EndDialog(hWnd, 0);
		}
		return TRUE;
	}
	return FALSE;
}

WM_HANDLER(InputBox_OnClose) {
	EndDialog(hWnd, 0);
	return TRUE;
}

BEGIN_HANDLER_MAP(inputbox_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, InputBox_OnCommand)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, InputBox_OnInitDialog)
	HANDLER_MAP_ENTRY(WM_CLOSE, InputBox_OnClose)
END_HANDLER_MAP

INT_PTR CALLBACK InputBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(inputbox_handlers, hWnd, uMsg, wParam, lParam);
}

bool InputBox(HWND hParent, const char * query, char * buf, size_t bufSize) {
	INPUTBOXPARAMS * ibp = (INPUTBOXPARAMS *)malloc(sizeof(INPUTBOXPARAMS));
	ibp->query = strdup(query);
	ibp->buf = buf;
	ibp->bufSize = bufSize;
	bool ret = false;
	if (DialogBoxParam(config.hInst, MAKEINTRESOURCE(IDD_INPUT), hParent, InputBoxProc, (LPARAM)ibp) == 1) {
		ret = true;
	}
	free(ibp->query);
	free(ibp);
	return ret;
}
