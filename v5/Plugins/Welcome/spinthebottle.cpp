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

#include "welcome.h"

static char spinbottlemessages[5][30] = {
	"The bottle is slowing down...",
	"The bottle stopped...",
	"It landed on...",
	"%target!",
	"%target must %action %nick!"
};
void DoSpinTheBottle(WEB_QUEUE * Scan) {
	char buf[256];
	if (time(NULL) >= Scan->req_time) {
		if (Scan->stage <= 4) {
			sprintf(buf, "SpinBottle%d", Scan->stage);
			if (!api->LoadMessage(buf, buf, sizeof(buf))) {
				strcpy(buf, spinbottlemessages[Scan->stage]);
			}
			str_replace(buf, sizeof(buf), "%chan", Scan->pres->Channel);
			str_replace(buf, sizeof(buf), "%action", Scan->data2);
			str_replace(buf, sizeof(buf), "%nick", Scan->pres->Nick);
			str_replace(buf, sizeof(buf), "%target", Scan->data);
			api->ProcText(buf, sizeof(buf));
			Scan->pres->send_chan(Scan->pres, buf);
			Scan->stage++;
			Scan->req_time = time(NULL) + 2;
		} else {
			Scan->nofree = false;
		}
	}
}
