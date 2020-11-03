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


#ifndef __DRIFT_WINDOWING_TOOLKIT_H__

#include <drift/dsl.h>

#define WM_HANDLER(x) INT_PTR CALLBACK x(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
#define HANDLE_MAP(x) return HandleMap(x, hWnd, uMsg, wParam, lParam)

#define BEGIN_HANDLER_MAP(x) HANDLERS x[] = {
#define END_HANDLER_MAP { 0, NULL } };
#define HANDLER_MAP_ENTRY(x,y){ x, y },

struct HANDLERS {
	UINT uMsg;
	DLGPROC proc;
};

INT_PTR CALLBACK HandleMap(HANDLERS * HandlerMap, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif // __DRIFT_WINDOWING_TOOLKIT_H__
