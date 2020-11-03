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

#include "ircbot.h"

class CommandAlias {
public:
	string command;
	string default_parms;
};
typedef std::map<std::string, CommandAlias, ci_less> commandAliasList;
commandAliasList Aliases;
Titus_Mutex aliasMutex;

int AliasCommandProc(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	aliasMutex.Lock();
	commandAliasList::const_iterator x = Aliases.find(command);
	if (x != Aliases.end()) {
		COMMAND * com = FindCommand(x->second.command.c_str());
		aliasMutex.Release();
		if (com) {
			char * pparms = NULL;
			if (x->second.default_parms.length() && (parms == NULL || parms[0] == 0)) {
				pparms = zstrdup(x->second.default_parms.c_str());
			}
			int ret = ExecuteProc(com, pparms ? pparms:parms, ut, type);
			zfreenn(pparms);
			return ret;
		}
	}
	aliasMutex.Release();
	return 0;
}

bool RegisterCommandAlias(const char * calias, const char * cmd, const char * parms) {
	aliasMutex.Lock();
	CommandAlias ca;
	ca.command = cmd;
	if (parms) {
		ca.default_parms = parms;
	}
	Aliases[calias] = ca;
	aliasMutex.Release();
	return true;
}

void RegisterCommandAliases() {
	aliasMutex.Lock();
	char buf[1024];
	for (commandAliasList::const_iterator x = Aliases.begin(); x != Aliases.end(); x++) {
		sprintf(buf, _("Alias for command %s"), x->second.command.c_str());
		COMMAND * com = FindCommand((char *)x->second.command.c_str());
		if (com) {
			if (com->proc_type == COMMAND_PROC) {
				RegisterCommand(x->first.c_str(), &com->acl, com->mask, AliasCommandProc, buf);
			} else {
				RegisterCommand(x->first.c_str(), &com->acl, com->mask, com->action, buf);
			}
		} else {
			ib_printf(_("%s: Error creating alias for %s -> %s, %s is not a valid command!\n"), IRCBOT_NAME, x->first.c_str(), x->second.command.c_str(), x->second.command.c_str());
		}
	}
	aliasMutex.Release();
}

void UnregisterCommandAliases() {
	aliasMutex.Lock();
	//char buf[1024];
	for (commandAliasList::const_iterator x = Aliases.begin(); x != Aliases.end(); x++) {
		UnregisterCommand(x->first.c_str());
	}
	Aliases.clear();
	aliasMutex.Release();
}

