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

#ifndef ENABLE_MYSQL
#define ENABLE_MYSQL
#endif
#include "../../src/plugins.h"
BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

DB_MySQL conx;

typedef struct {
	char hostname[256];
	char db[256];
	char key[32];
	long port;

	char user[256];
	char pass[256];

	bool ready;
} MySQL_Users_Config;

MySQL_Users_Config mu_config;

void ConnectMySQL() {
	conx.Disconnect();
	if (strlen(mu_config.hostname) && strlen(mu_config.key) && strlen(mu_config.user) && strlen(mu_config.db)) {
		int tries=0;
		while (tries < 5) {
			tries++;
			api->ib_printf("MySQL Users -> Connecting to MySQL Server... (try %d)\n", tries);
			if (conx.Connect(mu_config.hostname, mu_config.user, mu_config.pass, mu_config.db, mu_config.port)) {
				mu_config.ready = true;
				break;
			} else {
				safe_sleep(1);
			}
		}
		if (mu_config.ready) {
			conx.Query("CREATE TABLE IF NOT EXISTS `IRCBot_Users` (`ID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, `Nick` VARCHAR( 64 ) NOT NULL UNIQUE, `Pass` VARCHAR( 255 ) NOT NULL DEFAULT '', `Level` TINYINT NOT NULL DEFAULT '0', `Flags` INT UNSIGNED NOT NULL DEFAULT '0', `Seen` INT NOT NULL DEFAULT '0', `Created` INT NOT NULL DEFAULT '0');");
			conx.Query("ALTER TABLE `IRCBot_Users` MODIFY `Level` TINYINT NOT NULL DEFAULT '0'");
			conx.Query("CREATE TABLE IF NOT EXISTS `IRCBot_UserHostmasks` (`ID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, `Nick` VARCHAR( 64 ) NOT NULL DEFAULT '', `Hostmask` VARCHAR( 255 ) NOT NULL DEFAULT '', UNIQUE uniq_nick_hostmask (Nick,Hostmask));");
		} else {
			api->ib_printf(_("MySQL Users -> Could not connect to MySQL server, disabling MySQL support...\n"));
		}
	} else {
		api->ib_printf(_("MySQL Users -> Configuration incomplete, disabling MySQL support...\n"));
	}
}

void LoadConfig() {
	conx.Disconnect();
	memset(&mu_config,0,sizeof(mu_config));
	mu_config.port = 3306; // set the default port

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Users_MySQL");
	if (sec == NULL) { return; }

	char * p = api->config->GetConfigSectionValue(sec,"Server");
	if (p) {
		strlcpy(mu_config.hostname,p,sizeof(mu_config.hostname));
	}
	p = api->config->GetConfigSectionValue(sec,"DBName");
	if (p) {
		strlcpy(mu_config.db,p,sizeof(mu_config.db));
	}
	p = api->config->GetConfigSectionValue(sec,"EncKey");
	if (p) {
		strlcpy(mu_config.key,p,sizeof(mu_config.key));
	}
	p = api->config->GetConfigSectionValue(sec,"Port");
	if (p) {
		mu_config.port = atol(p);
	}
	if (mu_config.port == 0) { mu_config.port = 3306; }
	p = api->config->GetConfigSectionValue(sec,"User");
	if (p) {
		sstrcpy(mu_config.user,p);
	}
	p = api->config->GetConfigSectionValue(sec,"Pass");
	if (p) {
		sstrcpy(mu_config.pass,p);
	}

	ConnectMySQL();
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	memset(&mu_config, 0, sizeof(mu_config));
	LoadConfig();
	return 1;
}

void quit() {
	api->ib_printf(_("MySQL Users -> Shutting down...\n"));
	mu_config.ready = false;
	conx.Disconnect();
	api->ib_printf(_("MySQL Users -> Shutdown complete\n"));
}

int MySQL_Commands() {
	/*
	if (!strncmp(msg,"mysql-reload")) {
		char buf[1024];
		sprintf(buf,_("Reconnecting to MySQL..."));
		ConnectMySQL();
		return 1;
	}
	*/
	return 0;
}

bool auth_user(const char * hostmask, const char * nick, IBM_USEREXTENDED * ue) {
	char nbuf[256];
	conx.Ping();

	if (hostmask && hostmask[0]) {
		nick = NULL;
		stringstream query;
		query << "SELECT Nick FROM IRCBot_UserHostmasks WHERE '" << conx.EscapeString(hostmask) << "' LIKE Hostmask LIMIT 1";
		MYSQL_RES * res = conx.Query(query.str().c_str());
		if (res) {
			if (conx.NumRows(res) > 0) {
				SC_Row row;
				if (conx.FetchRow(res, row)) {
					sstrcpy(nbuf, row.Get("Nick").c_str());
					nick = nbuf;
				}
			}
			conx.FreeResult(res);
		}
	} else {
		strlcpy(nbuf, nick, sizeof(nbuf));
		nick = nbuf;
	}

	memset(ue, 0, sizeof(IBM_USEREXTENDED));
	if (nick) {
		stringstream query,query2,query3;
		query3 << "UPDATE IRCBot_Users SET Seen=" << time(NULL) << " WHERE Nick='" << conx.EscapeString(nick) << "'";
		conx.Query(query3.str().c_str());
		query << "SELECT *,AES_DECRYPT(Pass,'" << mu_config.key << "') AS dPass FROM IRCBot_Users WHERE Nick='" << conx.EscapeString(nick) << "'";
		//bool gotnick = false;
		MYSQL_RES * res = conx.Query(query.str().c_str());
		SC_Row row;
		if (res && conx.FetchRow(res, row)) {
			strlcpy(ue->nick, row.Get("Nick").c_str(), sizeof(ue->nick));
			strlcpy(ue->pass, row.Get("dPass").c_str(), sizeof(ue->nick));
			ue->flags = atoul(row.Get("Flags").c_str());
//			ue->seen = atoi64(conx.GetValue(&row, "Seen"));
			if (ue->flags == 0) {
				int level = atoi(row.Get("Level").c_str());
				if (level >= 1 && level <= 5) {
					ue->flags = api->user->GetLevelFlags(level);
				}
			}
			conx.FreeResult(res);

			query2 << "SELECT * FROM IRCBot_UserHostmasks WHERE Nick='" << conx.EscapeString(nick) << "'";
			res = conx.Query(query2.str().c_str());
			if (res && conx.NumRows(res)) {
				while (conx.FetchRow(res, row) && ue->numhostmasks < MAX_HOSTMASKS) {
					strlcpy(ue->hostmasks[ue->numhostmasks], row.Get("Hostmask").c_str(), sizeof(ue->hostmasks[ue->numhostmasks]));
					str_replaceA(ue->hostmasks[ue->numhostmasks], sizeof(ue->hostmasks[ue->numhostmasks]), "%", "*");
					ue->numhostmasks++;
				}
			}
			conx.FreeResult(res);
			return true;
		}
		conx.FreeResult(res);
	}
	return false;
}

bool add_user(IBM_USERFULL * ui) {
	conx.Ping();
	stringstream query,query2,query3,query4;
	query << "SELECT * FROM IRCBot_Users WHERE Nick='" << conx.EscapeString(ui->nick) << "'";
	//bool gotnick = false;
	MYSQL_RES * res = conx.Query(query.str().c_str());
	if (res && conx.NumRows(res)) {
		api->ib_printf(_("MySQL Users -> Error adding user '%s', already exists in MySQL DB!\n"), ui->nick);
		conx.FreeResult(res);
		return false;
	}
	conx.FreeResult(res);

	query4 << "DELETE FROM IRCBot_UserHostmasks WHERE Nick='" << conx.EscapeString(ui->nick) << "'";
	conx.Query(query4.str().c_str());
	query2 << "INSERT INTO IRCBot_Users SET Nick='" << conx.EscapeString(ui->nick) << "', Pass=AES_ENCRYPT('" << conx.EscapeString(ui->pass) << "','" << mu_config.key << "'), Flags=" << ui->flags << ", `Created`=" << time(NULL);
	conx.Query(query2.str().c_str());
	if (ui->hostmask[0]) {
		char * tmp = strdup(ui->hostmask);
		str_replaceA(tmp, strlen(tmp)+1, "*", "%");
		query3 << "REPLACE INTO IRCBot_UserHostmasks SET Nick='" << conx.EscapeString(ui->nick) << "', Hostmask='" << conx.EscapeString(tmp) << "'";
		free(tmp);
		conx.Query(query3.str().c_str());
	}
	return true;
}

int MsgProc(unsigned int msg, char * data, int datalen) {
	switch(msg) {
		case IB_GETUSERINFO:{
				IBM_GETUSERINFO * ui = (IBM_GETUSERINFO *)data;
				if (mu_config.ready) {
					api->ib_printf(_("MySQL Users -> Querying for %s (FETCH)...\n"), ui->ui.nick);
					if (auth_user(ui->ui.hostmask[0] ? ui->ui.hostmask:NULL, ui->ui.nick, &ui->ue)) {
						ui->type = IBM_UT_EXTENDED;
						return 1;
					}
				}
				return 0;
			}
			break;

		case IB_ADD_USER:{
			//return values are: 0 = I don't support this, 1 = Addition failed, 2 = Addition success
				IBM_USERFULL * ui = (IBM_USERFULL *)data;
				if (mu_config.ready == true) {
					if (add_user(ui)) {
						return 2;
					} else {
						return 1;
					}
				}
				return 1;
			}
			break;

		case IB_DEL_USER:{
				IBM_USERFULL * ui = (IBM_USERFULL *)data;
				if (mu_config.ready == true) {
					conx.Ping();
					stringstream query,query2;
					query << "DELETE FROM IRCBot_Users WHERE Nick='" << conx.EscapeString(ui->nick) << "'";
					conx.Query(query.str().c_str());
					query2 << "DELETE FROM IRCBot_UserHostmasks WHERE Nick='" << conx.EscapeString(ui->nick) << "'";
					conx.Query(query2.str().c_str());
				}
				return 0;
			}
			break;

		case IB_ADD_HOSTMASK:{
				IBM_USERFULL * ui = (IBM_USERFULL *)data;
				if (mu_config.ready == true) {
					conx.Ping();
					char * tmp = strdup(ui->hostmask);
					str_replaceA(tmp, strlen(tmp)+1, "*", "%");
					stringstream query;
					query << "REPLACE INTO IRCBot_UserHostmasks SET Nick='" << conx.EscapeString(ui->nick) << "', Hostmask='" << conx.EscapeString(tmp) << "'";
					free(tmp);
					conx.Query(query.str().c_str());
				}
				return 0;
			}
			break;

		case IB_DEL_HOSTMASK:{
				IBM_USERFULL * ui = (IBM_USERFULL *)data;
				if (mu_config.ready == true) {
					conx.Ping();
					char * tmp = strdup(ui->hostmask);
					str_replaceA(tmp, strlen(tmp)+1, "*", "%");
					stringstream query;
					query << "DELETE FROM IRCBot_UserHostmasks WHERE Nick='" << conx.EscapeString(ui->nick) << "' AND Hostmask='" << conx.EscapeString(tmp) << "'";
					free(tmp);
					conx.Query(query.str().c_str());
				}
				return 0;
			}
			break;

		case IB_DEL_HOSTMASKS:{
				IBM_USERFULL * ui = (IBM_USERFULL *)data;
				if (mu_config.ready == true) {
					conx.Ping();
					stringstream query;
					query << "DELETE FROM IRCBot_UserHostmasks WHERE Nick='" << conx.EscapeString(ui->nick) << "'";
					conx.Query(query.str().c_str());
				}
				return 0;
			}
			break;

		case IB_CHANGE_FLAGS:{
				IBM_USERFULL * ui = (IBM_USERFULL *)data;
				if (mu_config.ready == true) {
					conx.Ping();
					stringstream query;
					query << "UPDATE IRCBot_Users SET Flags=" << ui->flags << ", Level=0 WHERE Nick='" << conx.EscapeString(ui->nick) << "'";
					conx.Query(query.str().c_str());
				}
				return 0;
			}
			break;

		case IB_CHANGE_PASS:{
				IBM_USERFULL * ui = (IBM_USERFULL *)data;
				if (mu_config.ready == true) {
					conx.Ping();
					stringstream query;
					query << "UPDATE IRCBot_Users SET Pass=AES_ENCRYPT('" << conx.EscapeString(ui->pass) << "','" << mu_config.key << "') WHERE Nick='" << conx.EscapeString(ui->nick) << "'";
					conx.Query(query.str().c_str());
				}
				return 0;
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{99D38AD1-67FE-4F40-A7FB-D0A33854B8EE}",
	"MySQL",
	"MySQL User Authentication Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	MsgProc,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
