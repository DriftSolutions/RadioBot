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

#include "dwt.h"

INT_PTR CALLBACK HandleMap(HANDLERS * HandlerMap, HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	for (int i=0; HandlerMap[i].proc != NULL; i++) {
		if (HandlerMap[i].uMsg == uMsg) {
			INT_PTR ret = HandlerMap[i].proc(hWnd, uMsg, wParam, lParam);
			if (ret != FALSE) { return ret; }
		}
	}
	return FALSE;
}

