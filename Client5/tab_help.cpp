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

INT_PTR CALLBACK TabHelpProc(HWND, UINT, WPARAM, LPARAM);

HWND CreateTabHelp(HWND hParent) {
	HWND hWnd = CreateDialog(config.hInst, MAKEINTRESOURCE(IDD_HELP), hParent, TabHelpProc);
	return hWnd;
}

WM_HANDLER(Help_OnCommand) {
	if (LOWORD(wParam) == IDC_OPT_BEEP && HIWORD(wParam) == BN_CLICKED) {
		config.beep_on_req = (IsDlgButtonChecked(hWnd, IDC_OPT_BEEP) == BST_CHECKED) ? true:false;
	} else if (LOWORD(wParam) == IDC_OPT_KEEP && HIWORD(wParam) == BN_CLICKED) {
		config.keep_on_top = (IsDlgButtonChecked(hWnd, IDC_OPT_KEEP) == BST_CHECKED) ? true:false;
		SetWindowOnTop();
	}
	return TRUE;
}

WM_HANDLER(Help_OnInitDialog) {
	char buf[1024];
	char fn[1024];
	sstrcpy(fn, "client_help.txt");
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		char * tmp = new char[len+1];
		if (fread(tmp, len, 1, fp) == 1) {
			tmp[len] = 0;
			SetDlgItemText(hWnd, IDC_HELP_TEXT, tmp);
		} else {
			snprintf(buf, sizeof(buf), "Error reading '%s': %s", nopath(fn), strerror(errno));
			SetDlgItemText(hWnd, IDC_HELP_TEXT, buf);
		}
		delete [] tmp;
	} else {
		snprintf(buf, sizeof(buf), "Error opening '%s' for read: %s", nopath(fn), strerror(errno));
		SetDlgItemText(hWnd, IDC_HELP_TEXT, buf);
	}
	return TRUE;
}

BEGIN_HANDLER_MAP(tab_help_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, Help_OnCommand)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, Help_OnInitDialog)
END_HANDLER_MAP

INT_PTR CALLBACK TabHelpProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(tab_help_handlers, hWnd, uMsg, wParam, lParam);
}
