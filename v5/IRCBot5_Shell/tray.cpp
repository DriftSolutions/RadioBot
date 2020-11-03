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

void AddTrayIcon() {
	NOTIFYICONDATA nid;
	nid.cbSize=sizeof(nid);
	nid.hWnd=config.mWnd;
	nid.uID=1;
	nid.uVersion=NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_SETVERSION,&nid);

	nid.uFlags = NIF_ICON|NIF_TIP|NIF_MESSAGE;
	nid.hIcon = LoadIcon(config.hInstance,MAKEINTRESOURCE(IDI_ICON1));
	tcscpy(nid.szTip,TEXT("RadioBot v5 Shell"));
	nid.uTimeout=0;
	nid.uCallbackMessage=WM_USER+1;

	Shell_NotifyIcon(NIM_ADD,&nid);
	DeleteObject(nid.hIcon);
}

/*
void ChangeTrayIcon(int nIcon) {
	NOTIFYICONDATA nid;
	nid.cbSize=sizeof(nid);
	nid.hWnd=config.mWnd;
	nid.uID=1;
	nid.uVersion=NOTIFYICON_VERSION;
	Shell_NotifyIcon(NIM_SETVERSION,&nid);

	nid.uFlags = NIF_ICON;
	nid.hIcon = LoadIcon(config.hInstance,MAKEINTRESOURCE(IDI_ICON1));
	nid.uTimeout=0;
	nid.uCallbackMessage=WM_USER+27;

	if (!Shell_NotifyIcon(NIM_MODIFY,&nid)) {
		AddTrayIcon();
	}
	DeleteObject(nid.hIcon);
}
*/

void DelTrayIcon() {
	NOTIFYICONDATA nid;
	nid.cbSize=sizeof(nid);
	nid.hWnd=config.mWnd;
	nid.uID=1;
	nid.uFlags=0;
	nid.hIcon=NULL;
	Shell_NotifyIcon(NIM_DELETE,&nid);
}

void UpdateBalloon(tchar * strTitle, tchar * strContent, int showfor) {
	NOTIFYICONDATA nid;
	nid.cbSize=sizeof(nid);
	nid.hWnd=config.mWnd;
	nid.uID=1;

	nid.uFlags = NIF_INFO;
	memset(nid.szInfoTitle,0,sizeof(nid.szInfoTitle));
	tcsncpy(nid.szInfoTitle,strTitle,sizeof(nid.szInfoTitle)/sizeof(tchar));
	memset(nid.szInfo,0,sizeof(nid.szInfo));
	tcsncpy(nid.szInfo,strContent,sizeof(nid.szInfo)/sizeof(tchar));
	nid.uTimeout=(showfor * 1000);
	nid.dwInfoFlags=NIIF_INFO;
	nid.uCallbackMessage=WM_USER+1;
	if (!Shell_NotifyIcon(NIM_MODIFY,&nid)) {
		DelTrayIcon();
		AddTrayIcon();
		Shell_NotifyIcon(NIM_MODIFY,&nid);
	}
}
