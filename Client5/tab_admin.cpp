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

INT_PTR CALLBACK TabAdminProc(HWND, UINT, WPARAM, LPARAM);

HWND CreateTabAdmin(HWND hParent) {
	HWND hWnd = CreateDialog(config.hInst, MAKEINTRESOURCE(IDD_ADMIN), hParent, TabAdminProc);
	return hWnd;
}

WM_HANDLER(Admin_OnCommand) {
	if (LOWORD(wParam) == IDC_ADMIN_DIE) {
		REMOTE_HEADER head = { RCMD_DIE, 0 };
		SendPacket(&head, NULL);
	} else if (LOWORD(wParam) == IDC_ADMIN_RESTART) {
		REMOTE_HEADER head = { RCMD_RESTART, 0 };
		SendPacket(&head, NULL);
	} else if (LOWORD(wParam) == IDC_ADMIN_DOSPAM) {
		REMOTE_HEADER head = { RCMD_DOSPAM, 0 };
		SendPacket(&head, NULL);
	} else if (LOWORD(wParam) == IDC_ADMIN_BCAST) {
		char buf[512];
		if (InputBox(hWnd, "What text would you like to broadcast to all channels?", buf, sizeof(buf))) {
			REMOTE_HEADER head = { RCMD_BROADCAST_MSG, strlen(buf)+1 };
			SendPacket(&head, buf);
		}
	} else if (LOWORD(wParam) == IDC_ADMIN_CURDJ) {
		REMOTE_HEADER head = { RCMD_REQ_CURRENT, 0 };
		SendPacket(&head, NULL);
	} else if (LOWORD(wParam) == IDC_ADMIN_VERSION) {
		REMOTE_HEADER head = { RCMD_GET_VERSION, 0 };
		SendPacket(&head, NULL);
	} else if (LOWORD(wParam) == IDC_ADMIN_VIEWUSER) {
		char buf[512];
		if (InputBox(hWnd, "What user do you want to view?", buf, sizeof(buf))) {
			REMOTE_HEADER head = { RCMD_GETUSERINFO, strlen(buf)+1 };
			SendPacket(&head, buf);
		}
	}
	return TRUE;
}

WM_HANDLER(Admin_OnInitDialog) {
	return TRUE;
}

BEGIN_HANDLER_MAP(tab_admin_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, Admin_OnCommand)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, Admin_OnInitDialog)
END_HANDLER_MAP

INT_PTR CALLBACK TabAdminProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(tab_admin_handlers, hWnd, uMsg, wParam, lParam);
}
