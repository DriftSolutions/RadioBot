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

int LevelEmul_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "adduserlvl") && (!parms || parms[0] == 0)) {
			ut->std_reply(ut, _("Usage: adduserlvl nick level pass [hostmask]"));
			return 1;
	}
	if (!stricmp(command, "adduserlvl")) {
		char buf[1024],buf2[64];
		StrTokenizer st((char *)parms, ' ');
		if (st.NumTok() >= 3) {
			int level = atoi(st.stdGetSingleTok(2).c_str());
			if (level >= 1 && level <= 5) {
				if (!(ut->Flags & UFLAG_MASTER) && level == 1) {
					ut->std_reply(ut, _("You must have Master access to set a user to level 1!"));
					return 1;
				}
				api->user->uflag_to_string(api->user->GetLevelFlags(level), buf2, sizeof(buf2));
				snprintf(buf, sizeof(buf), "%s %s %s", st.stdGetSingleTok(1).c_str(), buf2, st.stdGetTok(3, st.NumTok()).c_str());
				COMMAND * com = api->commands->FindCommand("adduser");
				if (com) {
					api->commands->ExecuteProc(com, buf, ut, type);
				} else {
					ut->std_reply(ut, _("Error finding adduser command!"));
				}
			} else {
				ut->std_reply(ut, _("Error: level must be from 1-5!"));
			}
		}
		return 1;
	}

	if (!stricmp(command,"chlevel") && (!parms || !stricmp(parms,""))) {
		ut->std_reply(ut, _("Usage: chlevel nick level"));
		return 1;
	}

	if (!stricmp(command,"chlevel")) {
		char * cparms = zstrdup(parms);
		char * p2 = NULL;
		char *nick = strtok_r(cparms," ",&p2);
		if (nick == NULL) {
			ut->std_reply(ut, _("Usage: chlevel nick level"));
			zfree(cparms);
			return 1;
		}

		char *slvl = strtok_r(NULL, " ",&p2);
		if (slvl == NULL) {
			ut->std_reply(ut, _("Usage: chlevel nick level"));
			zfree(cparms);
			return 1;
		}

		int lvl = atoi(slvl);
		if (lvl < 1 || lvl > 5) {
			ut->std_reply(ut, _("Level must be between 1 and 5!"));
			zfree(cparms);
			return 1;
		}
		if (!(ut->Flags & UFLAG_MASTER) && lvl == 1) {
			ut->std_reply(ut, _("You must have Master access to set a user to level 1!"));
			zfree(cparms);
			return 1;
		}

		USER * user = api->user->GetUserByNick(nick);
		if (user) {
			api->user->SetUserFlags(user, api->user->GetLevelFlags(lvl));
			ut->std_reply(ut, _("User level updated successfully!"));
			UnrefUser(user);
		} else {
			ut->std_reply(ut, _("ERROR: Could not obtain user's record"));
		}
		zfree(cparms);
		return 1;
	}

	if (!stricmp(command, "level-uflags")) {
		char buf[256], buf2[64];
		for (int i=1; i <= 5; i++) {
			api->user->uflag_to_string(api->user->GetLevelFlags(i), buf2, sizeof(buf2));
			sprintf(buf, "Level %d: %s", i, buf2);
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	return 0;
}

void LoadConfig() {
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "LevelEmul");
	if (sec) {
		char buf[16];
		for (int i=1; i <= 5; i++) {
			sprintf(buf, "Level%d", i);
			const char * p = api->config->GetConfigSectionValue(sec, buf);
			if (p) {
				uint32 flags = api->user->GetLevelFlags(i);
				flags = api->user->string_to_uflag(p, flags);
				api->user->SetLevelFlags(i, flags);
			}
		}
	}
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = (BOTAPI_DEF *)BotAPI;
	pluginnum = num;
	LoadConfig();
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_OP|UFLAG_MASTER, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "level-uflags",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, LevelEmul_Commands, _("This will show you what flags will be assigned if you use chlevel or adduserlvl"));
	api->commands->RegisterCommand_Proc(pluginnum, "chlevel",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, LevelEmul_Commands, _("This will let you change a user's level"));
	api->commands->RegisterCommand_Proc(pluginnum, "adduserlvl",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, LevelEmul_Commands, _("This will let you add a user by user level instead of flags"));
	return 1;
}

void quit() {
	api->commands->UnregisterCommandsByPlugin(pluginnum);
}

int message(unsigned int msg, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{2C346734-101F-46CB-81CB-84B6ABC3E51E}",
	"LevelEmul",
	"IRCBot v1-v4 Level Emulation Plugin",
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
