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

#ifndef TIXML_USE_STL
#define TIXML_USE_STL
#endif
#include "../../src/plugins.h"
#include "../../../Common/tinyxml/tinyxml.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
bool shutdown_now = false;
time_t lastPublic=0;

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


void AddServer(const char * sname, const char * stype, const char * shost, int port) {
	std::stringstream sstr;
	char * name = sqlite_escape(sname);
	char * type = sqlite_escape(stype);
	char * host = sqlite_escape(shost);
	sstr << "REPLACE INTO game_servers(`Name`,`Type`,`Host`,`Port`) VALUES ('" << name << "', '" << type << "', '" << host << "', " << port << ")";
	api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
	zfree(name);
	zfree(host);
	zfree(type);
	//bSend(GetConfig()->ircnets[netno].sock, (char *)sstr.str().c_str());
}

struct QUERY {
	char name[128];
	char type[128];
	char host[128];
	int port;
	//time_t lastPublic;

	bool bPublic;
	USER_PRESENCE * pres;

	QUERY *Prev,*Next;
};

Titus_Mutex qMutex;
QUERY *qFirst=NULL, *qLast=NULL;

int Gaming_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {

	if ((!stricmp(command, "gstat") || !stricmp(command, "gstats") || !stricmp(command, "gstatus")) && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: gstat name or all [public]"));
		ut->std_reply(ut, _("Or: gservers [public] as a shortcut to gstat all [public]"));
		return 1;
	}

	if (!stricmp(command, "gstat") || !stricmp(command, "gstats") || !stricmp(command, "gstatus")) {
		StrTokenizer st((char *)parms, ' ');
		if (st.NumTok() == 1 || st.NumTok() == 2) {
			bool bPublic = (st.NumTok() >= 2 && !stricmp(st.stdGetSingleTok(2).c_str(), "public")) ? true:false;
			if (bPublic && lastPublic > (time(NULL)-30) && !(ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
				ut->std_reply(ut, _("You can only use public stats once every 30 seconds!"));
				return 1;
			}
			char * name = sqlite_escape(st.stdGetSingleTok(1).c_str());
			std::stringstream sstr2;
			if (!stricmp(name,"all")) {
				sstr2 << "SELECT * FROM game_servers ORDER BY `Name` ASC";
			} else {
				sstr2 << "SELECT * FROM game_servers WHERE `Name` LIKE '" << name << "'";
			}
			sql_rows rows = GetTable((char *)sstr2.str().c_str(), NULL);
			zfree(name);
			if (rows.size() > 0) {
				sql_rows::const_iterator x = rows.begin();
				while (x != rows.end()) {
					sql_row row = x->second;
					x++;

					QUERY * q = znew(QUERY);
					memset(q, 0, sizeof(QUERY));
					strcpy(q->name, row["Name"].c_str());
					strcpy(q->type, row["Type"].c_str());
					strcpy(q->host, row["Host"].c_str());
					q->port = atoi(row["Port"].c_str());

					RefUser(ut);
					q->pres = ut;
					if (bPublic && ut->Channel && ut->Channel[0]) {
						q->bPublic = true;
						lastPublic = time(NULL);
					} else {
						q->bPublic = false;
					}
					qMutex.Lock();
					if (qFirst) {
						qLast->Next = q;
						q->Prev = qLast;
						qLast = q;
					} else {
						qLast = qFirst = q;
					}
					qMutex.Release();
				}
			} else {
				ut->std_reply(ut, _("No server by that name was found."));
			}
		} else {
			ut->std_reply(ut, _("Usage: gstat name or all [public]"));
			ut->std_reply(ut, _("Or: gservers [public] as a shortcut to gstat all [public]"));
		}
		return 1;
	}

	if (!stricmp(command, "gservers")) {
		bool bPublic = false;
		if (parms && strlen(parms)) {
			StrTokenizer st((char *)parms, ' ');
			bPublic = (st.NumTok() >= 1 && !stricmp(st.stdGetSingleTok(1).c_str(), "public")) ? true:false;
		}
		if (bPublic) {
			if (!ut->Channel || !strlen(ut->Channel)) {
				ut->std_reply(ut, _("public only works in channels!"));
				return 1;
			}
			if (lastPublic > (time(NULL)-30) && !(ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
				ut->std_reply(ut, _("You can only use public stats once every 30 seconds!"));
				return 1;
			}
		}
		if (!bPublic && ut->Channel && strlen(ut->Channel)) {
			char buf[1024];
			sprintf(buf, _("I am now private messaging %s the game server stats..."), ut->Nick);
			ut->send_chan(ut, buf);
		}
		std::stringstream sstr2;
		sstr2 << "SELECT * FROM game_servers ORDER BY `Name` ASC";
		sql_rows rows = GetTable((char *)sstr2.str().c_str(), NULL);
		if (rows.size() > 0) {
			sql_rows::const_iterator x = rows.begin();
			while (x != rows.end()) {
				sql_row row = x->second;
				x++;

				QUERY * q = znew(QUERY);
				memset(q, 0, sizeof(QUERY));
				strcpy(q->name, row["Name"].c_str());
				strcpy(q->type, row["Type"].c_str());
				strcpy(q->host, row["Host"].c_str());
				q->port = atoi(row["Port"].c_str());

				q->pres = ut;
				RefUser(ut);
				if (bPublic) {
					q->bPublic = true;
					lastPublic = time(NULL);
				} else {
					q->bPublic = false;
				}
				qMutex.Lock();
				if (qFirst) {
					qLast->Next = q;
					q->Prev = qLast;
					qLast = q;
				} else {
					qLast = qFirst = q;
				}
				qMutex.Release();
			}
		} else {
			ut->std_reply(ut, _("No servers found."));
		}
		return 1;
	}

	if (!stricmp(command, "gsave") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: gsave name type hostname port"));
		ut->std_reply(ut, _("You can get a list of types from your copy of qstats or by going here: http://wiki.shoutirc.com/index.php/Plugin:Gaming"));
		return 1;
	}

	if (!stricmp(command, "gsave")) {
		StrTokenizer st((char *)parms, ' ');
		if (st.NumTok() == 4) {
			AddServer(st.stdGetSingleTok(1).c_str(), st.stdGetSingleTok(2).c_str(), st.stdGetSingleTok(3).c_str(), atoi(st.stdGetSingleTok(4).c_str()));
			ut->std_reply(ut, _("Server saved"));
		} else {
			ut->std_reply(ut, _("Usage: gsave name type hostname port"));
			ut->std_reply(ut, _("You can get a list of types from your copy of qstats or by going here: http://wiki.shoutirc.com/index.php/Plugin:Gaming"));
		}
		return 1;
	}

	if (!stricmp(command, "gdel") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: gdel name"));
		return 1;
	}

	if (!stricmp(command, "gdel")) {
		StrTokenizer st((char *)parms, ' ');
		if (st.NumTok() == 1) {
			char * name = sqlite_escape(st.stdGetSingleTok(1).c_str());
			std::stringstream sstr;
			sstr << "DELETE FROM game_servers WHERE `Name` LIKE '" << name << "'";
			api->db->Query((char *)sstr.str().c_str(), NULL, NULL, NULL);
			zfree(name);
			ut->std_reply(ut, _("Server deleted"));
		} else {
			ut->std_reply(ut, _("Usage: gdel name"));
		}
		return 1;
	}

	if (!stricmp(command, "glist")) {
		sql_rows rows = GetTable("SELECT * FROM game_servers ORDER By `Name` ASC", NULL);
		if (rows.size() > 0) {
			for (sql_rows::const_iterator x = rows.begin(); x != rows.end(); x++) {
				sql_row row = x->second;
				std::stringstream sstr;
				sstr << "Server '" << row["Name"] << "': " << row["Host"] << ":" << row["Port"] << "/" << row["Type"];
				ut->std_reply(ut, (char *)sstr.str().c_str());
			}
			ut->std_reply(ut, _("End of list."));
		} else {
			ut->std_reply(ut, _("No servers are defined..."));
		}
		return 1;
	}
	return 0;
}

struct GSTATS {
	bool online;
	int num_players, max_players;
	char map[128];
	char gametype[128];
	char name[256];
	char error[256];
	int ping;
};

bool IfExistCopy(TiXmlElement * m, char * name, char * buf, int bufSize) {
	TiXmlElement * e = m->FirstChildElement(name);
	if (e) {
		const char * p = e->GetText();
		if (p) {
			strlcpy(buf, p, bufSize);
			return true;
		}
	}
	return false;
}

int IfExistInt(TiXmlElement * m, char * name, int nDef = 0) {
	int ret = nDef;
	TiXmlElement * e = m->FirstChildElement(name);
	if (e) {
		const char * p = e->GetText();
		if (p) {
			if (!stricmp(p, "unlimited")) {
				ret = 999;
			} else {
				ret = atoi(p);
			}
		}
	}
	return ret;
}

THREADTYPE QueryThread(void * lpData) {
	TT_THREAD_START

	while (!shutdown_now) {
		qMutex.Lock();
		QUERY * Scan = qFirst;
		if (qFirst) { qFirst = qFirst->Next; }
		if (qFirst) { qFirst->Prev = NULL; }
		qMutex.Release();
		if (Scan) {
			std::stringstream sstr;
			sstr << "qstat -P -xml -of ./tmp/qstat.xml -" << Scan->type << "  " << Scan->host << ":" << Scan->port;
			system(sstr.str().c_str());

			GSTATS gs;
			memset(&gs, 0, sizeof(gs));

			TiXmlDocument doc;
			doc.LoadFile("./tmp/qstat.xml");
			TiXmlElement * root = doc.FirstChildElement("qstat");
			if (root) {
				TiXmlElement * srv = root->FirstChildElement("server");
				if (srv) {
					const char * val = srv->Attribute("status");
					if (val) {
						if (!stricmp(val,"UP")) {
							gs.online = true;
						} else {
							strlcpy(gs.error, val, sizeof(gs.error));
						}
					}
					IfExistCopy(srv, "name", gs.name, sizeof(gs.name)-1);
					IfExistCopy(srv, "map", gs.map, sizeof(gs.map)-1);
					IfExistCopy(srv, "gametype", gs.gametype, sizeof(gs.gametype)-1);
					if (!stricmp(gs.gametype,"tf") && !stricmp(Scan->type,"a2s")) {
						strcpy(gs.gametype, "Team Fortress");
					}
					TiXmlElement * sub = srv->FirstChildElement("numplayers");
					if (sub) {
						gs.num_players = atoi(sub->GetText());
					}
					sub = srv->FirstChildElement("maxplayers");
					if (sub) {
						gs.max_players = atoi(sub->GetText());
					}
					sub = srv->FirstChildElement("ping");
					if (sub) {
						gs.ping = atoi(sub->GetText());
					}
				}
			}
			remove("./tmp/qstat.xml");

			char buf[8192];
			memset(buf, 0, sizeof(buf));
			if (gs.online) {
				if (!api->LoadMessage("GameOnline", buf, sizeof(buf)-1)) {
					strcpy(buf, _("%bold[%boldServer Status: %name (%servip:%servport)%bold] [%boldName: %server%bold] [%boldGame Type: %gametype%bold] [%boldCurrent Map: %map%bold] [%boldPlayers: %numplayers/%maxplayers%bold] [%boldPing: %pingms%bold]"));
				}
			} else {
				if (!api->LoadMessage("GameOffline", buf, sizeof(buf)-1)) {
					strcpy(buf, _("%bold[%boldServer Status: %name%bold] [%boldError: %error%bold]"));
				}
			}

			str_replace(buf, sizeof(buf), "%name", Scan->name);
			str_replace(buf, sizeof(buf), "%servip", Scan->host);
			str_replace(buf, sizeof(buf), "%server", gs.name);
			str_replace(buf, sizeof(buf), "%map", gs.map);
			if (!strlen(gs.gametype)) {
				strcpy(gs.gametype, _("Unknown"));
			}
			str_replace(buf, sizeof(buf), "%gametype", gs.gametype);
			if (!strlen(gs.error)) {
				strcpy(gs.error, _("Unknown"));
			}
			str_replace(buf, sizeof(buf), "%error", gs.error);
			char buf2[16];
			sprintf(buf2, "%d", gs.ping);
			str_replace(buf, sizeof(buf), "%ping", buf2);
			sprintf(buf2, "%d", gs.num_players);
			str_replace(buf, sizeof(buf), "%numplayers", buf2);
			sprintf(buf2, "%d", gs.max_players);
			str_replace(buf, sizeof(buf), "%maxplayers", buf2);
			sprintf(buf2, "%d", Scan->port);
			str_replace(buf, sizeof(buf), "%servport", buf2);
			api->ProcText(buf, sizeof(buf));

			if (Scan->bPublic) {
				//std::stringstream sstr2;
				//sstr2 << "PRIVMSG " << Scan->spublic.to << " :" << buf << "\r\n";
				//api->SendIRC(Scan->spublic.netno, sstr2.str().c_str(), sstr2.str().length());
				Scan->pres->send_chan(Scan->pres, buf);
			} else {
				Scan->pres->send_msg(Scan->pres, buf);
			}

			UnrefUser(Scan->pres);
			zfree(Scan);
		}
		safe_sleep(1);
	}

	qMutex.Lock();
	while (qFirst) {
		QUERY * Scan = qFirst;
		qFirst = qFirst->Next;
		UnrefUser(Scan->pres);
		zfree(Scan);
	}
	qMutex.Release();

	TT_THREAD_END
};

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;

	api->db->Query("CREATE TABLE IF NOT EXISTS game_servers(`Name` VARCHAR(255) PRIMARY KEY, `Type` VARCHAR(255), `Host` VARCHAR(255), Port INTEGER DEFAULT 0);", NULL, NULL, NULL);
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "gstat",	&acl, CM_ALLOW_ALL, Gaming_Commands, _("This will let you query one of your game servers"));
	api->commands->RegisterCommand_Proc(pluginnum, "gstats",	&acl, CM_ALLOW_ALL, Gaming_Commands, _("This will let you query one of your game servers"));
	api->commands->RegisterCommand_Proc(pluginnum, "gstatus",	&acl, CM_ALLOW_ALL, Gaming_Commands, _("This will let you query one of your game servers"));
	api->commands->RegisterCommand_Proc(pluginnum, "gservers",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_IRC_PUBLIC					, Gaming_Commands, _("This will let you query all of your game servers"));
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "glist",	&acl, CM_ALLOW_ALL, Gaming_Commands, _("This will display all the server(s) you have defined with gsave"));
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "gsave",	&acl, CM_ALLOW_ALL, Gaming_Commands, _("This will let you save a server for use with gstat/gstats/gstatus"));
	api->commands->RegisterCommand_Proc(pluginnum, "gdel",		&acl, CM_ALLOW_ALL, Gaming_Commands, _("This will let you remove a saved game server from the database"));
	shutdown_now = false;
	TT_StartThread(QueryThread, NULL, _("Gaming Plugin Query Thread"), pluginnum);

	return 1;
}

void quit() {
	api->commands->UnregisterCommandsByPlugin(pluginnum);
	shutdown_now = true;
	while (TT_NumThreadsWithID(pluginnum)) {
		safe_sleep(100, true);
	}
}

int message(unsigned int msg, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{9F5701B0-2CE5-4b76-9CB9-42645C494284}",
	"Gaming",
	"Gaming Plugin 1.0",
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
