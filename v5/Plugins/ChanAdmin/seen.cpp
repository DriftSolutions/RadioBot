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

#include "chanadmin.h"

void update_channel_seen(USER_PRESENCE * ui) {
	char buf[512];
	int64 tme = time(NULL);
	sprintf(buf, "REPLACE INTO chanadmin_seen (Nick, Channel, User, NetNo, TimeStamp) VALUES ('%s', '%s', '%s', %d, " I64FMT ");", ui->Nick, ui->Channel, ui->User?ui->User->Nick:"", ui->NetNo, tme);
	api->db->Query(buf, NULL, NULL, NULL);
}

//	sprintf(buf, "REPLACE INTO chanadmin_seen (Nick, Channel, User, NetNo, TimeStamp) VALUES ('%s', '%s', '%s', %d, "I64FMT");", ui->nick, ui->channel, user?user->Nick:"", ui->netno, tme);
//	api->db->Query(buf, NULL, NULL, NULL);

int Seen_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[512],buf2[128];
	if ((!stricmp(command, "seen")) && (!parms || !strlen(parms))) {
		sprintf(buf, _("Usage: %cseen nick"), api->commands->PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "seen")) {
		//time_t tme = time(NULL);
		char * nick = api->db->MPrintf("%q", parms);
		stringstream str;
		str << "SELECT * FROM chanadmin_seen WHERE NetNo=" << ut->NetNo << " AND Channel LIKE '" << ut->Channel << "' AND Nick LIKE '" << nick << "'";
		api->db->Free(nick);
		sql_rows rows = GetTable(str.str().c_str(), NULL);
		sql_rows::iterator x = rows.begin();
		if (x != rows.end()) {
			tm tm1,tm2;
			time_t today = time(NULL);
			time_t seen = atoi64(x->second["TimeStamp"].c_str());
			localtime_r(&today, &tm1);
			localtime_r(&seen, &tm2);
			//memcpy(&tm1, localtime(&today), sizeof(tm1));
			//memcpy(&tm2, localtime(&seen), sizeof(tm2));
			if (tm1.tm_year == tm2.tm_year && tm1.tm_yday == tm2.tm_yday) {
				strftime(buf2, sizeof(buf2), "\002today\002, %I:%M:%S %p", &tm2);
			} else if (tm1.tm_year == tm2.tm_year && tm2.tm_yday == (tm1.tm_yday-1)) {
				strftime(buf2, sizeof(buf2), "\002yesterday\002, %I:%M:%S %p", &tm2);
			} else if (seen == 0) {
				strcpy(buf2, "never");
			} else {
				strftime(buf2, sizeof(buf2), "%B %d, %Y %I:%M:%S %p", &tm2);
			}
			sprintf(buf,_("%s was last seen in %s: %s"), x->second["Nick"].c_str(), ut->Channel, buf2);
		} else {
			sprintf(buf,_("I have not seen %s in %s..."), parms, ut->Channel);
		}
		ut->std_reply(ut, buf);
		return 1;
	}

	return 0;
}
