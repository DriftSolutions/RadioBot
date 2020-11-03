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


//COMMAND * EnumCommandsByLevel(COMMAND * Scan, int32 level);
COMMAND * EnumCommandsByFlags(COMMAND * Scan, uint32 flags, uint32 flags_not, uint32 mask, int plugfilt=-2);
COMMAND * EnumCommandsByFlags2(COMMAND * Start, uint32 user_flags, uint32 group_flags, uint32 groups_used, uint32 mask, int plugfilt=-2);
COMMAND * FindCommand(const char * command);

int ExecuteProc(COMMAND * com, const char * parms, USER_PRESENCE * ut, uint32 type);
bool command_is_allowed(COMMAND * com, uint32 flags);
void RegisterCommandHook(const char * command, CommandProcType hook_func);
void UnregisterCommandHook(const char * command, CommandProcType hook_func);

COMMAND * RegisterCommand2(int pluginnum, const char * command, const COMMAND_ACL * acl, uint32 mask, const char * action, const char * desc);
COMMAND * RegisterCommand2(int pluginnum, const char * command, const COMMAND_ACL * acl, uint32 mask, CommandProcType proc, const char * desc);
#define RegisterCommand(a,b,c,d,e) RegisterCommand2(-1,a,b,c,d,e)
COMMAND_ACL * FillACL(COMMAND_ACL * acl, uint32 flags_have_all, uint32 flags_have_any, uint32 flags_not);
COMMAND_ACL MakeACL(uint32 flags_have_all, uint32 flags_have_any, uint32 flags_not);

void UnregisterCommand(const char * command);
void UnregisterCommand(COMMAND * Scan);
void UnregisterCommands();
void UnregisterCommandsByPlugin(int pluginnum);
void UnregisterCommandsWithMask(uint32 mask);

void RegisterInternalCommands();
//void InitCommandPool();
//void ShutdownCommandPool();
//int CommandOutput_IRC_PM(T_SOCKET * sock, const char * dest, const char * str);
//int CommandOutput_IRC_Notice(T_SOCKET * sock, const char * dest, const char * str);
