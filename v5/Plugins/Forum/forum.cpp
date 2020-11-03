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

#include "forums.h"

BOTAPI_DEF *api=NULL;
int pluginnum;
//SQLConx conx;
FORUM_CONFIG forum_config;

extern FORUM_DRIVER driver_phpbb;
extern FORUM_DRIVER driver_vbulletin;
FORUM_DRIVER * forum_drivers[] = {
	&driver_phpbb,
	&driver_vbulletin,
	NULL
};

FORUM_DRIVER * GetForumDriver(const char * name) {
	for (int i=0; forum_drivers[i] != NULL; i++) {
		if (!stricmp(name, forum_drivers[i]->name)) {
			return forum_drivers[i];
		}
	}
	return NULL;
}

bool LoadConfig() {
	memset(&forum_config, 0, sizeof(forum_config));
	DS_CONFIG_SECTION * root = api->config->GetConfigSection(NULL, "Forum");
	if (root == NULL) {
		api->ib_printf(_("Forum -> Error: You do not have a Forum section!\n"));
		return false;
	}

	bool ret = false;
	char buf[64];
	for (int i=0; i < MAX_FORUMS; i++) {
		forum_config.forums[i].id = i;
		sprintf(buf, "Account%d", i);
		DS_CONFIG_SECTION * sec = api->config->GetConfigSection(root, buf);
		if (sec != NULL) {
			memset(buf, 0, sizeof(buf));
			api->config->GetConfigSectionValueBuf(sec, "Type", buf, sizeof(buf));
			forum_config.forums[i].driver = GetForumDriver(buf);
			if (forum_config.forums[i].driver) {
				api->config->GetConfigSectionValueBuf(sec, "Host", forum_config.forums[i].db.host, sizeof(forum_config.forums[i].db.host));
				api->config->GetConfigSectionValueBuf(sec, "User", forum_config.forums[i].db.user, sizeof(forum_config.forums[i].db.user));
				api->config->GetConfigSectionValueBuf(sec, "Pass", forum_config.forums[i].db.pass, sizeof(forum_config.forums[i].db.pass));
				api->config->GetConfigSectionValueBuf(sec, "DBName", forum_config.forums[i].db.dbname, sizeof(forum_config.forums[i].db.dbname));
				api->config->GetConfigSectionValueBuf(sec, "TablePrefix", forum_config.forums[i].tabprefix, sizeof(forum_config.forums[i].tabprefix));
				int n = api->config->GetConfigSectionLong(sec, "Port");
				if (n > 0 && n < 65536) {
					forum_config.forums[i].db.port = n;
				} else {
					forum_config.forums[i].db.port = 3306;
				}

				n = api->config->GetConfigSectionLong(sec, "UserID");
				if (n <= 0) {
					api->ib_printf(_("Forum -> Error: You have not set a valid UserID in Forum/Account%d!\n"), i);
					return false;
				}
				forum_config.forums[i].me.user_id = n;

				n = api->config->GetConfigSectionLong(sec, "ForumID");
				if (n > 0) {
					forum_config.forums[i].forum_id = n;
				}
				n = api->config->GetConfigSectionLong(sec, "StatusPostID");
				if (n > 0) {
					forum_config.forums[i].status_post_id = n;
				}

				ret = true;
			} else {
				api->ib_printf(_("Forum -> Error: Unknown forum type in Forum/Account%d: %s!\n"), i, buf);
			}
		}
	}

	if (!ret) {
		api->ib_printf(_("Forum -> Error: No forum accounts defined!\n"));
	}
	return ret;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api=BotAPI;
	pluginnum=num;
	api->ib_printf(_("Forum -> Loading...\n"));

	if (api->ss == NULL) {
		api->ib_printf2(pluginnum,_("Forum -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	if (!LoadConfig()) {
		return false;
	}

	stringstream str,str2;
	str << "CREATE TABLE IF NOT EXISTS phpBB (Account INTEGER, Type VARCHAR(16), ID VARCHAR(255))";
	api->db->Query(str.str().c_str(), NULL, NULL, NULL);
	str2 << "CREATE UNIQUE INDEX IF NOT EXISTS phpBBUnique ON phpBB(Account, Type)";
	api->db->Query(str2.str().c_str(), NULL, NULL, NULL);

	TT_StartThread(Forum_Posts, NULL, "Forum Thread", pluginnum);
	return true;
}

void quit() {
	api->ib_printf(_("Forum -> Shutting down...\n"));
	forum_config.shutdown_now=true;
	while (TT_NumThreadsWithID(pluginnum)) {
		api->ib_printf(_("Forum -> Waiting on threads to die...\n"));
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}
	api->commands->UnregisterCommandsByPlugin(pluginnum);
	api->ib_printf(_("Forum -> Shut down.\n"));
}

int message(unsigned int MsgID, char * data, int datalen) {
	switch(MsgID) {
#if !defined(IRCBOT_LITE)
		case IB_SS_DRAGCOMPLETE:{
				bool *b = (bool *)data;
				if (*b == true) {
					UpdateStatusPosts();
				}
			}
			break;
#endif
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{7EAA7AA4-74B2-42F1-8B99-26584515E54F}",
	"Forum",
	"Forum Integration Plugin",
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
