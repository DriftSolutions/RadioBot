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

bool handle_onjoin_bans(USER_PRESENCE * ui) {
	if (!(ui->Flags & UFLAG_MASTER)) {
		char buf[1024];
		int64 tme = time(NULL);
		sprintf(buf, "SELECT Hostmask,Reason FROM chanadmin_bans WHERE ((NetNo=%d AND Channel LIKE '%s') OR Channel LIKE 'global') AND (TimeExpire=0 OR TimeExpire>" I64FMT ");", ui->NetNo, ui->Channel, tme);
		sql_rows rows = GetTable(buf, NULL);
		for (sql_rows::iterator x = rows.begin(); x != rows.end(); x++) {
			if (wildicmp(x->second["Hostmask"].c_str(), ui->Hostmask)) {
				do_ban(ui->NetNo, ui->Channel, ui->Hostmask);
				string r = x->second["Reason"];
				do_kick(ui->NetNo, ui->Channel, ui->Nick, r.length() ? r.c_str():"banned");
				return true;
			}
		}
	}
	return false;
}

bool do_kick(int netno, const char * chan, const char * nick, const char * reason) {
	stringstream str;
	if (reason == NULL) { reason = "banned"; }
	str << "KICK " << chan << " " << nick << " :" << reason << "\r\n";
	api->irc->SendIRC_Priority(netno, str.str().c_str(), str.str().length(), PRIORITY_IMMEDIATE);
	return true;
}
bool do_ban(int netno, const char * chan, const char * hostmask) {
	stringstream str;
	str << "MODE " << chan << " +b " << hostmask << "\r\n";
	api->irc->SendIRC_Priority(netno, str.str().c_str(), str.str().length(), PRIORITY_IMMEDIATE);
	return true;
}
bool do_unban(int netno, const char * chan, const char * hostmask) {
	stringstream str;
	str << "MODE " << chan << " -b " << hostmask << "\r\n";
	api->irc->SendIRC_Priority(netno, str.str().c_str(), str.str().length(), PRIORITY_IMMEDIATE);
	return true;
}

bool add_ban(int netno, const char * chan, const char * hostmask, const char * set_by, int64 time_expire, const char * reason) {
	int64 tme = time(NULL);
	char buf[1024];
	sprintf(buf, "INSERT INTO chanadmin_bans (NetNo, Channel, Hostmask, SetBy, TimeSet, TimeExpire, Reason) VALUES (%d, '%s', '%s', '%s', " I64FMT ", " I64FMT ", '%s');", netno, chan, hostmask, set_by, tme, time_expire, reason?reason:"");
	if (api->db->Query(buf, NULL, NULL, NULL) == SQLITE_OK) {
		if (stricmp(chan, "global") && api->ial->is_bot_in_chan(netno, chan)) {
			do_ban(netno, chan, hostmask);
			char * nick = zstrdup(hostmask);
			char * p = strchr(nick, '!');
			if (p) { *p = 0; }
			if (api->ial->is_nick_in_chan(netno, chan, nick)) {
				do_kick(netno, chan, nick, reason);
			}
			zfree(nick);
		}
		return true;
	} else {
		return false;
	}
}
bool del_ban(int netno, const char * chan, const char * hostmask) {
	char buf[1024];
	if (!stricmp(chan, "global")) {
		sprintf(buf, "DELETE FROM chanadmin_bans WHERE Hostmask LIKE '%s' AND Channel LIKE 'global'", hostmask);
	} else {
		sprintf(buf, "DELETE FROM chanadmin_bans WHERE NetNo=%d AND Channel LIKE '%s' AND Hostmask LIKE '%s'", netno, chan, hostmask);
	}
	api->db->Query(buf, NULL, NULL, NULL);

	if (stricmp(chan, "global") && api->ial->is_bot_in_chan(netno, chan)) {
		do_unban(netno, chan, hostmask);
	}
	return true;
}
void del_expired_bans() {
	stringstream str,str2;
	time_t tme = time(NULL);
	str << "SELECT * FROM chanadmin_bans WHERE TimeExpire>0 AND TimeExpire<" << tme;
	sql_rows rows = GetTable(str.str().c_str(), NULL);
	for (sql_rows::iterator x = rows.begin(); x != rows.end(); x++) {
		int netno = atoi(x->second["NetNo"].c_str());
		if (api->ial->is_bot_in_chan(netno, x->second["Channel"].c_str())) {
			del_ban(netno, x->second["Channel"].c_str(), x->second["Hostmask"].c_str());
		}
	}
}

int Bans_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];
	if ((!stricmp(command, "ban")) && (!parms || !strlen(parms))) {
		sstrcpy(buf, _("Usage: ban #channel/global add|del|list|clear"));
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, "ban")) {
		char * cmd2 = NULL;
		char * tmp = zstrdup(parms);
		char * chan = strtok_r(tmp, " ", &cmd2);
		if (chan) {
			char * cmd = strtok_r(NULL, " ", &cmd2);
			if (cmd) {
				if (!stricmp(cmd, "add")) {
					char * hm = strtok_r(NULL, " ", &cmd2);
					if (hm) {
						StrTokenizer st((char *)parms, ' ');
						int64 te = 0;
						string reason;
						int netno = ut->NetNo;
						for (unsigned long i=4; i < st.NumTok(); i++) {
							string opt = st.stdGetSingleTok(i);
							if (!stricmp(opt.c_str(), "+exp") || !stricmp(opt.c_str(), "+expire") || !stricmp(opt.c_str(), "+expires")) {
								i++;
								te = time(NULL) + api->textfunc->decode_duration(st.stdGetSingleTok(i).c_str());
							} else if (!stricmp(opt.c_str(), "+net") || !stricmp(opt.c_str(), "+netno")) {
								i++;
								netno = atoi(st.stdGetSingleTok(i).c_str());
								if (netno < 0 || netno >= api->irc->NumNetworks()) {
									ut->std_reply(ut, _("Network number out of range, using your current netno instead..."));
									netno = ut->NetNo;
								}
							} else {
								reason = st.stdGetTok(i, st.NumTok());
							}
						}
						if (add_ban(netno, chan, hm, ut->User ? ut->User->Nick:ut->Nick, te, reason.c_str())) {
							sprintf(buf, _("%s added to ban list"), hm);
							ut->std_reply(ut, buf);
						} else {
							sprintf(buf, _("Error adding %s to ban list"), hm);
							ut->std_reply(ut, buf);
						}
					} else {
						sstrcpy(buf, _("Usage: ban #channel/global add hostmask [+exp 1h] [+net netno] [reason]"));
						ut->std_reply(ut, buf);
					}
				} else if (!stricmp(cmd, "del")) {
					char * hm = strtok_r(NULL, " ", &cmd2);
					if (hm) {
						int netno = ut->NetNo;
						StrTokenizer st((char *)parms, ' ');
						for (unsigned long i=4; i < st.NumTok(); i++) {
							string opt = st.stdGetSingleTok(i);
							if (!stricmp(opt.c_str(), "+net") || !stricmp(opt.c_str(), "+netno")) {
								i++;
								netno = atoi(st.stdGetSingleTok(i).c_str());
								if (netno < 0 || netno >= api->irc->NumNetworks()) {
									ut->std_reply(ut, _("Network number out of range, using your current netno instead..."));
									netno = ut->NetNo;
								}
							}
						}
						del_ban(netno, chan, hm);
						sprintf(buf, _("%s deleted from %s ban list"), hm, chan);
						ut->std_reply(ut, buf);
					} else {
						sstrcpy(buf, _("Usage: ban #channel/global del hostmask [+net netno]"));
						ut->std_reply(ut, buf);
					}
				} else if (!stricmp(cmd, "list")) {
					int64 tme = time(NULL);
					int netno = ut->NetNo;
					StrTokenizer st((char *)parms, ' ');
					for (unsigned long i=3; i < st.NumTok(); i++) {
						string opt = st.stdGetSingleTok(i);
						if (!stricmp(opt.c_str(), "+net") || !stricmp(opt.c_str(), "+netno")) {
							i++;
							netno = atoi(st.stdGetSingleTok(i).c_str());
							if (netno < 0 || netno >= api->irc->NumNetworks()) {
								ut->std_reply(ut, _("Network number out of range, using your current netno instead..."));
								netno = ut->NetNo;
							}
						}
					}
					if (!stricmp(chan, "global")) {
						sprintf(buf, "SELECT * FROM chanadmin_bans WHERE Channel LIKE 'global' AND (TimeExpire=0 OR TimeExpire>" I64FMT ")", tme);
					} else {
						sprintf(buf, "SELECT * FROM chanadmin_bans WHERE NetNo=%d AND Channel LIKE '%s' AND (TimeExpire=0 OR TimeExpire>" I64FMT ")", netno, chan, tme);
					}
					sql_rows rows = GetTable(buf, NULL);

					char buf2[64], buf3[64];
					int num = 0;
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
						sprintf(buf, _("Ban %d: %s [set by %s on %s] [expiration: %s]"), num, x->second["Hostmask"].c_str(), x->second["SetBy"].c_str(), buf2, buf3);
						ut->std_reply(ut, buf);
					}
					ut->std_reply(ut, _("End of list."));
				} else if (!stricmp(cmd, "clear")) {
					int netno = ut->NetNo;
					StrTokenizer st((char *)parms, ' ');
					for (unsigned long i=3; i < st.NumTok(); i++) {
						string opt = st.stdGetSingleTok(i);
						if (!stricmp(opt.c_str(), "+net") || !stricmp(opt.c_str(), "+netno")) {
							i++;
							netno = atoi(st.stdGetSingleTok(i).c_str());
							if (netno < 0 || netno >= api->irc->NumNetworks()) {
								ut->std_reply(ut, _("Network number out of range, using your current netno instead..."));
								netno = ut->NetNo;
							}
						}
					}
					if (!stricmp(chan, "global")) {
						sprintf(buf, "DELETE FROM chanadmin_bans WHERE Channel LIKE 'global'");
					} else {
						sprintf(buf, "DELETE FROM chanadmin_bans WHERE NetNo=%d AND Channel LIKE '%s'", netno, chan);
					}
					api->db->Query(buf, NULL, NULL, NULL);
					ut->std_reply(ut, _("Cleared list"));
				} else {
					sstrcpy(buf, _("Usage: ban #channel add|del|list|clear [parms]"));
					ut->std_reply(ut, buf);
				}
			} else {
				sstrcpy(buf, _("Usage: ban #channel add|del|list|clear [parms]"));
				ut->std_reply(ut, buf);
			}
		} else {
			sstrcpy(buf, _("Usage: ban #channel add|del|list|clear [parms]"));
			ut->std_reply(ut, buf);
		}
		zfree(tmp);
		return 1;
	}

	return 0;
}
