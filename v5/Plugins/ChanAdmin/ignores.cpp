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

bool is_ignored(USER_PRESENCE * ui) {
	if (!(ui->Flags & UFLAG_MASTER)) {
		char buf[1024];
		int64 tme = time(NULL);
		sprintf(buf, "SELECT Hostmask,Reason FROM chanadmin_ignores WHERE (TimeExpire=0 OR TimeExpire>" I64FMT ");", tme);
		sql_rows rows = GetTable(buf, NULL);
		for (sql_rows::iterator x = rows.begin(); x != rows.end(); x++) {
			if (wildicmp(x->second["Hostmask"].c_str(), ui->Hostmask)) {
				string r = x->second["Reason"];
				api->ib_printf(_("ChanAdmin -> Ignoring message from %s due to matching ignore '%s' (%s)\n"), ui->Nick, x->second["Hostmask"].c_str(), r.length() ? r.c_str():"no reason specified");
				return true;
			}
		}
	}
	return false;
}

bool add_ignore(const char * hostmask, const char * set_by, int64 time_expire, const char * reason) {
	int64 tme = time(NULL);
	char buf[1024];
	sprintf(buf, "INSERT INTO chanadmin_ignores (Hostmask, SetBy, TimeSet, TimeExpire, Reason) VALUES ('%s', '%s', " I64FMT ", " I64FMT ", '%s');", hostmask, set_by, tme, time_expire, reason?reason:"");
	if (api->db->Query(buf, NULL, NULL, NULL) == SQLITE_OK) {
		return true;
	} else {
		return false;
	}
}
bool del_ignore(const char * hostmask) {
	char buf[1024];
	sprintf(buf, "DELETE FROM chanadmin_ignores WHERE Hostmask LIKE '%s'", hostmask);
	api->db->Query(buf, NULL, NULL, NULL);
	USER * user = api->user->GetUser(hostmask);
	if (user) {
		api->user->DelUserFlags(user, UFLAG_IGNORE);
		UnrefUser(user);
	}
	return true;
}
void del_expired_ignores() {
	char buf[1024];
	int64 tme = time(NULL);
	sprintf(buf, "SELECT Hostmask,Reason FROM chanadmin_ignores WHERE (TimeExpire>0 OR TimeExpire<" I64FMT ");", tme);
	sql_rows rows = GetTable(buf, NULL);
	for (sql_rows::iterator x = rows.begin(); x != rows.end(); x++) {
		USER * user = api->user->GetUser(x->second["Hostmask"].c_str());
		if (user) {
			api->user->DelUserFlags(user, UFLAG_IGNORE);
			UnrefUser(user);
		}
	}

	stringstream str;
	str << "DELETE FROM chanadmin_ignores WHERE TimeExpire>0 AND TimeExpire<" << tme;
	api->db->Query(str.str().c_str(), NULL, NULL, NULL);
}

int Ignores_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];
	if ((!stricmp(command, "ignore") || !stricmp(command, "unignore")) && (!parms || !strlen(parms))) {
		sprintf(buf, _("Usage: %s add|del|list|clear"), command);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "ignore")) {
		char * cmd2 = NULL;
		char * tmp = zstrdup(parms);
		char * cmd = strtok_r(tmp, " ", &cmd2);
		if (cmd) {
			if (!stricmp(cmd, "add")) {
				char * hm = strtok_r(NULL, " ", &cmd2);
				if (hm) {
					int64 te = 0;
					char * exp = strtok_r(NULL, " ", &cmd2);
					char * reason = NULL;
					if (exp) {
						if (exp[0] == '+') {
							te = time(NULL) + api->textfunc->decode_duration(exp+1);
							reason = strtok_r(NULL, " ", &cmd2);
						} else {
							reason = exp;
						}
					}
					if (add_ignore(hm, ut->User ? ut->User->Nick:ut->Nick, te, reason)) {
						sprintf(buf, _("%s added to ignore list"), hm);
						ut->std_reply(ut, buf);
					} else {
						sprintf(buf, _("Error adding %s to ignore list"), hm);
						ut->std_reply(ut, buf);
					}
				} else {
					sprintf(buf, _("Usage: %s add hostmask [expiration ex: +1h] [reason]"), command);
					ut->std_reply(ut, buf);
				}
			} else if (!stricmp(cmd, "del")) {
				char * hm = strtok_r(NULL, " ", &cmd2);
				if (hm) {
					del_ignore(hm);
					sprintf(buf, _("%s deleted from ignore list"), hm);
					ut->std_reply(ut, buf);
				} else {
					sprintf(buf, _("Usage: %s del hostmask"), command);
					ut->std_reply(ut, buf);
				}
			} else if (!stricmp(cmd, "list")) {
				int64 tme = time(NULL);
				sprintf(buf, "SELECT * FROM chanadmin_ignores WHERE (TimeExpire=0 OR TimeExpire>" I64FMT ")", tme);
				sql_rows rows = GetTable(buf, NULL);

				char buf2[64], buf3[64];
				int num = 0;
				sprintf(buf, _("Number of ignores: %u"), rows.size());
				ut->std_reply(ut, buf);
				for (sql_rows::iterator x = rows.begin(); x != rows.end(); x++) {
					num++;
					time_t tme2 = atoi64(x->second["TimeSet"].c_str());
					strcpy(buf2, ctime(&tme2));
					strtrim(buf2);
					tme2 = atoi64(x->second["TimeExpire"].c_str());
					if (tme2 == 0) {
						strcpy(buf3, "never");
					} else if ((tme2-time(NULL)) <= (86400*90)) {
						api->textfunc->duration(tme2-time(NULL), buf3);
						//strftime(buf3, ctime(&tme2));
					} else {
						strcpy(buf3, ctime(&tme2));
						strtrim(buf3);
					}
					sprintf(buf, _("Ignore %d: %s [set by %s on %s] [expiration: %s] [reason: %s]"), num, x->second["Hostmask"].c_str(), x->second["SetBy"].c_str(), buf2, buf3, x->second["Reason"].c_str());
					ut->std_reply(ut, buf);
				}
				ut->std_reply(ut, _("End of list."));
			} else if (!stricmp(cmd, "clear")) {
				sprintf(buf, "DELETE FROM chanadmin_ignores");
				api->db->Query(buf, NULL, NULL, NULL);
				ut->std_reply(ut, _("Cleared list"));
			} else {
				sprintf(buf, _("Usage: %s add|del|list|clear [parms]"), command);
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf, _("Usage: %s add|del|list|clear [parms]"), command);
			ut->std_reply(ut, buf);
		}
		zfree(tmp);
		return 1;
	}

	return 0;
}
