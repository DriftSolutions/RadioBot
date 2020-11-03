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

const char * lists[6] = {
	"vop",
	"hop",
	"aop",
	"pop",
	"oop",
	NULL
};

const char listmode[6] = {
	'v',
	'h',
	'o',
	'a',
	'q',
	0
};

void handle_onjoin_automodes(USER_PRESENCE * ui) {
	if (ui->User) {
		char table[64];
		char buf[1024];
		for (int i=0; lists[i] != 0; i++) {
			sprintf(table, "chanadmin_%s", lists[i]);
			//strlwr(table);
			char * query = api->db->MPrintf("SELECT * FROM %s WHERE Nick LIKE '%q' AND Channel LIKE '%q';", table, ui->User->Nick, ui->Channel);
			char ** result;
			int nrows=0, ncols=0;
			//sql_rows rows = GetTable(buf, NULL);
			if (api->db->GetTable(query, &result, &nrows, &ncols, NULL) == SQLITE_OK) {
				if (result) { api->db->FreeTable(result); }
				if (nrows > 0) {
					snprintf(buf, sizeof(buf), "MODE %s +%c %s\r\n", ui->Channel, listmode[i], ui->Nick);
					api->irc->SendIRC_Priority(ui->NetNo, buf, strlen(buf), PRIORITY_INTERACTIVE);
				}
			}
			api->db->Free(query);
		}
	}
}

int AutoMode_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];
	if ((!stricmp(command, "vop") || !stricmp(command, "hop") || !stricmp(command, "aop") || !stricmp(command, "pop") || !stricmp(command, "oop")) && (!parms || !strlen(parms))) {
		sprintf(buf, _("Usage: %s #channel add|del|list|clear [nick]"), command);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "vop") || !stricmp(command, "hop") || !stricmp(command, "aop") || !stricmp(command, "pop") || !stricmp(command, "oop")) {
		char table[64];
		sprintf(table, "chanadmin_%s", command);
		strlwr(table);
		char * cmd2 = NULL;
		char * tmp = zstrdup(parms);
		char * chan = strtok_r(tmp, " ", &cmd2);
		if (chan) {
			char * cmd = strtok_r(NULL, " ", &cmd2);
			if (cmd) {
				if (!stricmp(cmd, "add")) {
					char * nick = strtok_r(NULL, " ", &cmd2);
					if (nick) {
						if (api->user->IsValidUserName(nick)) {
							/*
							sprintf(buf, "DELETE FROM chanadmin_vop Where Channel LIKE '%s' AND Nick LIKE '%s';", chan, nick);
							api->db->Query(buf, NULL, NULL, NULL);
							sprintf(buf, "DELETE FROM chanadmin_hop Where Channel LIKE '%s' AND Nick LIKE '%s';", chan, nick);
							api->db->Query(buf, NULL, NULL, NULL);
							sprintf(buf, "DELETE FROM chanadmin_aop Where Channel LIKE '%s' AND Nick LIKE '%s';", chan, nick);
							api->db->Query(buf, NULL, NULL, NULL);
							sprintf(buf, "DELETE FROM chanadmin_pop Where Channel LIKE '%s' AND Nick LIKE '%s';", chan, nick);
							api->db->Query(buf, NULL, NULL, NULL);
							sprintf(buf, "DELETE FROM chanadmin_oop Where Channel LIKE '%s' AND Nick LIKE '%s';", chan, nick);
							api->db->Query(buf, NULL, NULL, NULL);
							*/
							char * query = api->db->MPrintf("DELETE FROM %s WHERE Channel LIKE '%q' AND Nick LIKE '%q';", table, chan, nick);
							api->db->Query(query, NULL, NULL, NULL);
							api->db->Free(query);
							int32 ts = time(NULL);
							query = api->db->MPrintf("INSERT INTO %s (Nick, Channel, TimeStamp) VALUES ('%q', '%q', %d);", table, nick, chan, ts);
							if (api->db->Query(query, NULL, NULL, NULL) == SQLITE_OK) {
								sprintf(buf, _("%s added to %s %s list"), nick, chan, command);
								ut->std_reply(ut, buf);
							} else {
								sprintf(buf, _("Error adding user to %s list"), command);
								ut->std_reply(ut, buf);
							}
							api->db->Free(query);
						} else {
							sstrcpy(buf, _("Error: I do not know that username. Only registered usernames can be used in this list!"));
							ut->std_reply(ut, buf);
							sstrcpy(buf, _("Note: If a user's account (ie. !adduser/!viewuser) is Chad, but they are using the nick BillyBob, you must use Chad."));
							ut->std_reply(ut, buf);
						}
					} else {
						sprintf(buf, _("Usage: %s #channel add nick"), command);
						ut->std_reply(ut, buf);
					}
				} else if (!stricmp(cmd, "del")) {
					char * nick = strtok_r(NULL, " ", &cmd2);
					if (nick) {
						char * query = api->db->MPrintf("DELETE FROM %s WHERE Channel LIKE '%q' AND Nick LIKE '%q';", table, chan, nick);
						api->db->Query(query, NULL, NULL, NULL);
						api->db->Free(query);
						sprintf(buf, _("%s deleted from %s %s list"), nick, chan, command);
						ut->std_reply(ut, buf);
					} else {
						sprintf(buf, _("Usage: %s #channel del nick"), command);
						ut->std_reply(ut, buf);
					}
				} else if (!stricmp(cmd, "list")) {
					char * query = api->db->MPrintf("SELECT Nick FROM %s WHERE Channel LIKE '%q';", table, chan);
					char ** result;
					int nrows=0, ncols=0;
					if (api->db->GetTable(query, &result, &nrows, &ncols, NULL) == SQLITE_OK) {
						sprintf(buf, _("Users in list: %d"), nrows);
						ut->std_reply(ut, buf);

						for (int i=0; i < nrows; i++) {
							ut->std_reply(ut, result[i+1]);
						}

						if (result) { api->db->FreeTable(result); }
					} else {
						ut->std_reply(ut, _("Error retrieving list!"));
					}
					api->db->Free(query);
				} else if (!stricmp(cmd, "clear")) {
					char * query = api->db->MPrintf("DELETE FROM %s Where Channel LIKE '%q';", table, chan);
					api->db->Query(buf, NULL, NULL, NULL);
					api->db->Free(query);
					ut->std_reply(ut, _("Cleared list"));
				} else {
					sprintf(buf, _("Usage: %s #channel add|del|list|clear [nick]"), command);
					ut->std_reply(ut, buf);
				}
			} else {
				sprintf(buf, _("Usage: %s #channel add|del|list|clear [nick]"), command);
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf, _("Usage: %s #channel add|del|list|clear [nick]"), command);
			ut->std_reply(ut, buf);
		}
		zfree(tmp);
		return 1;
	}

	return 0;
}
