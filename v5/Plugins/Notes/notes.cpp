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

#include "../../src/plugins.h"
//#include <boost/shared_ptr.hpp>

BOTAPI_DEF * api = NULL;
int pluginnum = 0;

char * sqlite_escape(const char * str) {
	char * ret = (char *)zmalloc(1);
	ret[0]=0;
	size_t rlen = 0;
	size_t len = strlen(str);
	for (size_t i=0; i < len; i++) {
		ret = (char *)zrealloc(ret, rlen+2);
		ret[rlen] = str[i];
		rlen++;
		if (str[i] == '\'') {
			ret = (char *)zrealloc(ret, rlen+2);
			ret[rlen] = str[i];
			rlen++;
		}
		ret[rlen] = 0;
	}
	return ret;
}

sql_rows GetTable(char * query, char **errmsg) {
//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = api->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		api->ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
	} else {
		if (ncol && nrow) {
			for (int i=0; i < nrow; i++) {
				sql_row row;
				for (int s=0; s < ncol; s++) {
					char * p = resultp[(i*ncol)+ncol+s];
					if (!p) { p=""; }
					row[resultp[s]] = p;
				}
				rows[i] = row;
			}
		}
		api->db->FreeTable(resultp);
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}


void AddNote(const char * from, const char * to, const char * subject, const char * content) {
	std::stringstream sstr;
	char * sub = sqlite_escape(subject);
	char * con = sqlite_escape(content);
	sstr << "INSERT INTO notes (`From`,`To`,`Subject`,`Content`,`TimeStamp`) VALUES ('" << from << "', '" << to << "', '" << sub << "', '" << con << "', " << time(NULL) << ")";
	api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
	zfree(con);
	zfree(sub);
	//bSend(GetConfig()->ircnets[netno].sock, (char *)sstr.str().c_str());
}

int Notes_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "remember") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: remember text"));
		return 1;
	}

	if (!stricmp(command, "remember")) {
		AddNote(ut->User->Nick, ut->User->Nick, _("Remember Note"), parms);
		ut->std_reply(ut, _("I will remember that for you"));
		return 1;
	}

	if (!stricmp(command, "notes") && (!parms || !strlen(parms))) {
		ut->send_msg(ut, _("Usage: notes list/read/send/del"));
		return 1;
	}

	if (!stricmp(command, "notes")) {
		//char * nick = (char *)api->GetUserName(ut->hostmask);
		StrTokenizer st((char *)parms, ' ');
		string cmd = st.stdGetSingleTok(1);
		if (!stricmp(cmd.c_str(), "list")) {
			char * query = zmprintf("SELECT COUNT(*), SUM(Read) FROM notes WHERE `To` LIKE '%s'", ut->User->Nick);
			sql_rows info = GetTable(query, NULL);
			zfree(query);

			std::stringstream sstr;
			sstr << "SELECT ID,Subject,`From`,Read FROM notes WHERE `To` LIKE '" << ut->User->Nick << "'";
			sql_rows res = GetTable((char *)sstr.str().c_str(), NULL);

			if (res.size()) {
				char * buf = zmprintf(_("Number of messages: %s (%s read)"), info[0]["COUNT(*)"].c_str(), info[0]["SUM(Read)"].c_str());
				ut->send_msg(ut, buf);
				zfree(buf);
				ut->send_msg(ut, "");
				for (size_t i=0; i < res.size(); i++) {
					buf = zmprintf(_("%s. \'%s\' from %s [%s]"), res[i]["ID"].c_str(), res[i]["Subject"].c_str(), res[i]["From"].c_str(), stricmp(res[i]["Read"].c_str(), "1") == 0 ? "Read":"New");
					ut->send_msg(ut, buf);
					zfree(buf);
				}
			} else {
				ut->send_msg(ut, _("You have no messages."));
			}
			//api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
		} else if (!stricmp(cmd.c_str(), "read")) {
			if (st.NumTok() >= 2) {
				string num = st.stdGetSingleTok(2);
				std::stringstream sstr;
				if (!stricmp(num.c_str(), "all")) {
					sstr << "SELECT * FROM notes WHERE `To` LIKE '" << ut->User->Nick << "'";
				} else {
					sstr << "SELECT * FROM notes WHERE ID=" << num << " AND `To` LIKE '" << ut->User->Nick << "'";
				}
				sql_rows res = GetTable((char *)sstr.str().c_str(), NULL);
				if (res.size()) {
					for (size_t i=0; i < res.size(); i++) {

						time_t tme = atoi64(res[i]["TimeStamp"].c_str());
						//time(&tme);
						tm tml;
						localtime_r(&tme, &tml);
						char ib_time[512];
						strftime(ib_time, sizeof(ib_time), _("%m/%d/%y %I:%M:%S %p"), &tml);

						char * buf = zmprintf(_("Message %s: \'%s\' from %s. Sent at %s."), res[i]["ID"].c_str(), res[i]["Subject"].c_str(), res[i]["From"].c_str(), ib_time);
						ut->send_msg(ut, buf);
						ut->send_msg(ut, "");
						ut->send_msg(ut, (char *)res[i]["Content"].c_str());
						ut->send_msg(ut, "");
						zfree(buf);

						if (stricmp(res[i]["Read"].c_str(), "1")) {
							buf = zmprintf("UPDATE notes SET Read='1' WHERE ID=%s", res[i]["ID"].c_str());
							api->db->Query(buf, NULL, NULL, NULL);
							zfree(buf);
						}
					}
				} else {
					ut->send_msg(ut, _("I could not find any messages matching your query"));
				}
			} else {
				ut->send_msg(ut, _("Usage: notes read # or all"));
			}
		} else if (!stricmp(cmd.c_str(), "del")) {
			if (st.NumTok() >= 2) {
				string num = st.stdGetSingleTok(2);
				std::stringstream sstr;
				if (!stricmp(num.c_str(), "all")) {
					sstr << "DELETE FROM notes WHERE `To` LIKE '" << ut->User->Nick << "'";
				} else {
					sstr << "DELETE FROM notes WHERE ID=" << num << " AND `To` LIKE '" << ut->User->Nick << "'";
				}
				api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
				ut->send_msg(ut, _("Message(s) deleted!"));
			} else {
				ut->send_msg(ut, _("Usage: notes del # or all"));
			}
		} else if (!stricmp(cmd.c_str(), "send")) {
			if (st.NumTok() >= 3) {
				string to = st.stdGetSingleTok(2);
				string text = st.stdGetTok(3,st.NumTok());
				if (api->user->IsValidUserName(to.c_str())) {
					char * buf = zmprintf(_("Note from %s"), ut->Nick);
					AddNote(ut->Nick, to.c_str(), buf, text.c_str());
					zfree(buf);
					ut->send_msg(ut, _("Message sent!"));
				} else {
					char * buf = zmprintf(_("I could not find anyone with the nick '%s' (note: this is the internal RadioBot username, not the person's IRC nick, although they are probably the same in most cases)"), to.c_str());
					ut->send_msg(ut, buf);
					zfree(buf);
				}
			} else {
				ut->send_msg(ut, _("Usage: notes send user text"));
			}
		} else {
			ut->send_msg(ut, _("Usage: notes list/read/send/del"));
		}
		return 1;
	}

	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = (BOTAPI_DEF *)BotAPI;
	pluginnum = num;
	char buf[4096];
	strcpy(buf, "CREATE TABLE IF NOT EXISTS notes(ID INTEGER PRIMARY KEY AUTOINCREMENT, `From` VARCHAR(255), `To` VARCHAR(255), Subject VARCHAR(255), Content TEXT, TimeStamp INTEGER, Read INTEGER DEFAULT 0, Extra1 TEXT, Extra2 TEXT);");
	api->db->Query(buf, NULL, NULL, NULL);
	strcpy(buf, "CREATE TABLE IF NOT EXISTS note_reminders(ID INTEGER PRIMARY KEY AUTOINCREMENT, Nick VARCHAR(255), TimeStamp INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_DJ|UFLAG_HOP|UFLAG_OP|UFLAG_MASTER, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "notes",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, Notes_Commands, _("This will let you manage your notes, or send new ones"));
	api->commands->RegisterCommand_Proc(pluginnum, "remember", &acl, CM_ALLOW_ALL, Notes_Commands, _("This will record a note to yourself based on what you type after it"));
	return 1;
}

void quit() {
	api->commands->UnregisterCommandByName( "remember" );
	api->commands->UnregisterCommandByName( "notes" );
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
		case IB_ONJOIN:{
				USER_PRESENCE * ui = (USER_PRESENCE *)data;
				USER * user = ui->User;
				if (user) {
					char * query = zmprintf("SELECT * FROM note_reminders WHERE `Nick` LIKE '%s'", user->Nick);
					sql_rows info = GetTable(query, NULL);
					zfree(query);

					bool doAnnounce = false;
					if (info.size() == 0) {
						doAnnounce = true;
					} else {
						time_t tme = atoi64(info[0]["TimeStamp"].c_str());
						if ((time(NULL) - tme) >= 3600) {
							doAnnounce = true;
						}
					}

					if (doAnnounce) {
						query = zmprintf("SELECT COUNT(*), SUM(Read) FROM notes WHERE `To` LIKE '%s'", user->Nick);
						sql_rows res = GetTable(query, NULL);
						zfree(query);

						int32 cnt = atoi(res[0]["COUNT(*)"].c_str());
						int32 read = atoi(res[0]["SUM(Read)"].c_str());
						if (cnt > read) {
							if (info.size() == 0) {
								long tme = time(NULL);
								char * query = zmprintf("INSERT INTO note_reminders (`Nick`,`TimeStamp`) VALUES ('%s', '%d')", user->Nick, tme);
								api->db->Query(query, NULL, NULL, NULL);
								zfree(query);
							} else {
								long tme = time(NULL);
								char * query = zmprintf("UPDATE note_reminders SET `TimeStamp`=%d Where ID=%s", tme, info[0]["ID"].c_str());
								api->db->Query(query, NULL, NULL, NULL);
								zfree(query);
							}
							if (api->irc) {
								char * buf = zmprintf("PRIVMSG %s :You have %d unread messages in the notes plugin. Type /msg %s !notes list to view your message list\r\n", ui->Nick, cnt - read, api->irc->GetCurrentNick(ui->NetNo));
								api->irc->SendIRC(ui->NetNo, buf, strlen(buf));
								zfree(buf);
							}
						}
					}
				}
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{0387296A-5DE6-4adf-9936-D25400F72971}",
	"Notes",
	"Note Keeping Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
