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

#include "ircbot.h"


int SendMessage(int32 toplug, uint32 MsgID, char * data, int32 datalen) {
	if (toplug == -1) {
		for (int pln=0; pln < MAX_PLUGINS; pln++) {
			if (config.plugins[pln].hHandle && config.plugins[pln].plug && config.plugins[pln].plug->message) {
				int ret = config.plugins[pln].plug->message(MsgID, data, datalen);
				if (ret != 0) { return ret; }
			}
		}
	} else {
		PLUGIN_PUBLIC * Scan = GetPlugin(toplug);
		if (Scan && Scan->message != NULL) {
			return Scan->message(MsgID, data, datalen);
		}
	}
	return 0;
}
