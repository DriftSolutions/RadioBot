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

#define DSL_STATIC
#define MEMLEAK
#include <drift/dsl.h>
#include <Windowsx.h>
#include "resource.h"
#include "../../Common/DriftWindowingToolkit/dwt.h"

struct CONFIG {
	HINSTANCE hInstance;
	HWND mWnd;
	UINT trayCreated;
	HBRUSH logBG;
	char fnConf[MAX_PATH];
	bool manual_stop;

	T_SOCKET * lSock;
	T_SOCKET * cSock[2];
	int port;

	bool start_bot_at_startup;
	bool keep_bot_running;
	time_t startBotAt;

	struct {
		bool bot_running;
		HANDLE hStdOut;
		PROCESS_INFORMATION pi;
	} proc;
};
extern CONFIG config;

void AddTrayIcon();
void DelTrayIcon();
void UpdateBalloon(tchar * strTitle, tchar * strContent, int showfor);
