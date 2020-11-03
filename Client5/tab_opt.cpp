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

INT_PTR CALLBACK TabOptionsProc(HWND, UINT, WPARAM, LPARAM);

HWND CreateTabOptions(HWND hParent) {
	HWND hWnd = CreateDialog(config.hInst, MAKEINTRESOURCE(IDD_OPTIONS), hParent, TabOptionsProc);
	return hWnd;
}

WM_HANDLER(Options_OnCommand) {
	if (LOWORD(wParam) == IDC_OPT_BEEP && HIWORD(wParam) == BN_CLICKED) {
		config.beep_on_req = (IsDlgButtonChecked(hWnd, IDC_OPT_BEEP) == BST_CHECKED) ? true:false;
	} else if (LOWORD(wParam) == IDC_OPT_KEEP && HIWORD(wParam) == BN_CLICKED) {
		config.keep_on_top = (IsDlgButtonChecked(hWnd, IDC_OPT_KEEP) == BST_CHECKED) ? true:false;
		SetWindowOnTop();
	} else if (LOWORD(wParam) == IDC_OPT_AUTOCONN && HIWORD(wParam) == BN_CLICKED) {
		config.connect_on_startup = (IsDlgButtonChecked(hWnd, IDC_OPT_AUTOCONN) == BST_CHECKED) ? true:false;
	}
	return TRUE;
}

WM_HANDLER(Options_OnInitDialog) {
	CheckDlgButton(hWnd, IDC_OPT_KEEP, config.keep_on_top ? BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(hWnd, IDC_OPT_BEEP, config.beep_on_req ? BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(hWnd, IDC_OPT_AUTOCONN, config.connect_on_startup ? BST_CHECKED:BST_UNCHECKED);
	return TRUE;
}

BEGIN_HANDLER_MAP(tab_options_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, Options_OnCommand)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, Options_OnInitDialog)
END_HANDLER_MAP

INT_PTR CALLBACK TabOptionsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(tab_options_handlers, hWnd, uMsg, wParam, lParam);
}
