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
#include "commands_priv.h"

Titus_Mutex comMutex;
commandList commands;

HOOKS *fHook=NULL, *lHook=NULL;

bool IsValidCommand(COMMAND * com) {
	if (comMutex.LockingThread() != GetCurrentThreadId()) {
		ib_printf(_("%s: comMutex should be locked when calling IsValidCommand\n"), IRCBOT_NAME);
	}
	for (commandList::const_iterator x = commands.begin(); x != commands.end(); x++) {
		if (*x == com) {
			return true;
		}
	}
	return false;
}

bool command_is_allowed(COMMAND * com, uint32 flags) {
	if ((flags & com->acl.flags_all) != com->acl.flags_all) {
		return false;
	}
	if (com->acl.flags_any && (flags & com->acl.flags_any) == 0) {
		return false;
	}
	if ((flags & com->acl.flags_not) != 0) {
		return false;
	}
	return true;
}

TT_DEFINE_THREAD(ExecuteProcAsync) {
	TT_THREAD_START
	ASYNC_PROC * ap = (ASYNC_PROC *)tt->parm;
	//ExecuteProc(ap->com, ap->parms, ap->ut, ap->type);

	comMutex.Lock();
	bool done = false;
	HOOKS * Scan = fHook;
	while (Scan && !done) {
		if (!stricmp(Scan->command, ap->com->command)) {
			int ret = Scan->proc(ap->com->command, ap->parms, ap->ut, ap->type);
			if (ret) {
				done = true;
			}
		}
		Scan = Scan->Next;
	}
	comMutex.Release();

	if (!done) {
		ap->com->proc(ap->com->command, ap->parms, ap->ut, ap->type);
	}

	UnrefUser(ap->ut);
	comMutex.Lock();
	ap->com->ref_cnt--;
#if defined(DEBUG)
	ib_printf("Command '%s' refcnt-- = %u\n", ap->com->command, ap->com->ref_cnt);
#endif
	comMutex.Release();
	zfreenn(ap->parms);
	zfree(ap);
	TT_THREAD_END
}

int ExecuteProc(COMMAND * com, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (ut->User) { UpdateUserSeen(ut); }
	comMutex.Lock();
	if (!IsValidCommand(com)) {
		comMutex.Release();
		ib_printf(_("%s: ExecuteProc called with unrecognized command pointer!\n"), IRCBOT_NAME);
		return 0;
	}
	if (!command_is_allowed(com, ut->Flags)) {
		ib_printf(_("%s: %s tried to use the command %s that they don't have access to!\n"), IRCBOT_NAME, ut->Nick, com->command);
		comMutex.Release();
		return 0;
	}
	if (com->proc_type != COMMAND_PROC) {
		ib_printf(_("%s: Command %s is not of type COMMAND_PROC!\n"), IRCBOT_NAME, com->command);
		comMutex.Release();
		return 0;
	}

	//com->mask |= CM_FLAG_ASYNC;
	if (com->mask & CM_FLAG_ASYNC) {
		ASYNC_PROC * ap = znew(ASYNC_PROC);
		memset(ap, 0, sizeof(ASYNC_PROC));
		ap->com = com;
		ap->parms = parms ? zstrdup(parms):NULL;
		ap->type = type;
		RefUser(ut);
		ap->ut = ut;
		com->ref_cnt++;
#if defined(DEBUG)
		ib_printf("Command '%s' refcnt++ = %u\n", com->command, com->ref_cnt);
#endif
		comMutex.Release();

		TT_StartThread(ExecuteProcAsync, ap);
		//execQueue.push_back(ap);
		return 0;
	}

	HOOKS * Scan = fHook;
	while (Scan) {
		if (!stricmp(Scan->command,com->command)) {
			int ret = Scan->proc(com->command, parms, ut, type);
			if (ret) {
				comMutex.Release();
				return ret;
			}
		}
		Scan = Scan->Next;
	}

	com->ref_cnt++;
#if defined(DEBUG)
	ib_printf("Command '%s' refcnt++ = %u\n", com->command, com->ref_cnt);
#endif
	comMutex.Release();
	int ret = com->proc(com->command, parms, ut, type);
	comMutex.Lock();
	com->ref_cnt--;
#if defined(DEBUG)
	ib_printf("Command '%s' refcnt-- = %u\n", com->command, com->ref_cnt);
#endif
	comMutex.Release();
	/*
	for (int x=0; x < 25; x++) {
		com->proc(com->command, parms, ut, type);
	}
	*/
	return ret;
}

void RegisterCommandHook(const char * command, CommandProcType hook_func) {
	HOOKS * Scan = (HOOKS *)zmalloc(sizeof(HOOKS));
	memset(Scan, 0, sizeof(HOOKS));
	Scan->command = zstrdup(command);
	Scan->proc = hook_func;

	comMutex.Lock();
	if (fHook) {
		lHook->Next = Scan;
		Scan->Prev = lHook;
		lHook = Scan;
	} else {
		fHook = Scan;
		lHook = Scan;
	}
	comMutex.Release();
}

HOOKS * FindHook(const char * command, CommandProcType hook_func) {
	comMutex.Lock();
	if (comMutex.LockingThread() != GetCurrentThreadId()) {
		ib_printf(_("%s: comMutex should be locked when calling FindHook\n"), IRCBOT_NAME);
	}
	HOOKS * Scan = fHook;
	while (Scan) {
		if (!stricmp(Scan->command, command) && Scan->proc == hook_func) {
			comMutex.Release();
			return Scan;
		}
		Scan = Scan->Next;
	}
	comMutex.Release();
	return NULL;
};

void UnregisterCommandHook(const char * command, CommandProcType hook_func) {
	HOOKS * Scan = NULL;
	comMutex.Lock();
	while ((Scan = FindHook(command, hook_func))) {
		if (Scan == fHook && Scan == lHook) {
			fHook = NULL;
			lHook = NULL;
		} else if (Scan == fHook) {
			fHook = fHook->Next;
			fHook->Prev = NULL;
		} else if (Scan == lHook) {
			lHook = lHook->Prev;
			lHook->Next = NULL;
		} else {
			Scan->Next->Prev = Scan->Prev;
			Scan->Prev->Next = Scan->Next;
		}
		zfree((void *)Scan->command);
		zfree(Scan);
	}
	comMutex.Release();
}

void UnregisterCommandHooks() {
	comMutex.Lock();
	while (fHook) {
		UnregisterCommandHook(fHook->command, fHook->proc);
	}
	comMutex.Release();
}

/*
COMMAND * EnumCommandsByLevel(COMMAND * Start, int32 level) {
	comMutex.Lock();
	if (comMutex.LockingThread() != GetCurrentThreadId()) {
		ib_printf(_("%s: comMutex should be locked when calling FindCommand with lock=false\n"), IRCBOT_NAME);
	}
	if (!Start || !IsValidCommand(Start)) {
		if (fCmd->minlevel == level) {
			comMutex.Release();
			return fCmd;
		}
		Start = fCmd;
	}

	COMMAND * Scan = Start->Next;

	while (Scan) {
		if (Scan->minlevel == level) {
			comMutex.Release();
			return Scan;
		}
		Scan = Scan->Next;
	}

	comMutex.Release();
	return NULL;
}
*/

commandList::iterator FindCommand(COMMAND * Scan) {
	for (commandList::iterator x = commands.begin(); x != commands.end(); x++) {
		if (*x == Scan) {
			return x;
		}
	}
	return commands.end();
}

#if defined(DEBUG)
void DumpCommands() {
	AutoMutex(comMutex);
	FILE * fp = fopen("commands.txt","wb");
	if (fp != NULL) {
		for (commandList::iterator x = commands.begin(); x != commands.end(); x++) {
			fprintf(fp, "%s\n", (*x)->command);
		}
		fclose(fp);
	} else {
		ib_printf("Error opening commands.txt for write!\n");
	}
}
#endif

COMMAND * EnumCommandsByFlags(COMMAND * Start, uint32 flags, uint32 flags_not, uint32 mask, int plugfilt) {
	comMutex.Lock();
	if (comMutex.LockingThread() != GetCurrentThreadId()) {
		ib_printf(_("%s: comMutex should be locked when calling FindCommand with lock=false\n"), IRCBOT_NAME);
	}

	commandList::iterator x;// = (Start != NULL) ? FindCommand(Start):commands.begin();
	if (Start == NULL) {
		x = commands.begin();
	} else {
		x = FindCommand(Start);
		if (x != commands.end()) {
			x++;
		}
	}
	while (x != commands.end()) {
		COMMAND * com = *x;
		if (flags && !((com->acl.flags_all|com->acl.flags_any) & flags)) {
			x++;
			continue;
		}
		if ((com->acl.flags_all|com->acl.flags_any) & flags_not) {
			x++;
			continue;
		}
		if (com->acl.flags_not & flags) {
			x++;
			continue;
		}
		if ((flags != 0 && flags != 0xFFFFFFFF) && (com->acl.flags_all|com->acl.flags_any) == 0) {
			x++;
			continue;
		}
		if (mask && !(com->mask & mask)) {
			x++;
			continue;
		}
		if (plugfilt != -2 && com->plugin != plugfilt) {
			x++;
			continue;
		}

		comMutex.Release();
		return *x;
	}

	comMutex.Release();
	return NULL;
}

COMMAND * EnumCommandsByFlags2(COMMAND * Start, uint32 user_flags, uint32 group_flags, uint32 groups_used, uint32 mask, int plugfilt) {
	comMutex.Lock();

	commandList::iterator x;// = (Start != NULL) ? FindCommand(Start):commands.begin();
	if (Start == NULL) {
		x = commands.begin();
	} else {
		x = FindCommand(Start);
		if (x != commands.end()) {
			x++;
		}
	}
	while (x != commands.end()) {
		COMMAND * com = *x;
		if (!command_is_allowed(com, user_flags)) {
			x++;
			continue;
		}
		if (group_flags && !((com->acl.flags_all|com->acl.flags_any) & group_flags)) {
			x++;
			continue;
		}
		if ((com->acl.flags_all|com->acl.flags_any) & groups_used) {
			x++;
			continue;
		}
		if (com->acl.flags_not & group_flags) {
			x++;
			continue;
		}
		if ((group_flags != 0 && group_flags != 0xFFFFFFFF) && (com->acl.flags_all|com->acl.flags_any) == 0) {
			x++;
			continue;
		}
		ib_printf("%s mask: %u / %u\n", com->command, mask, com->mask);
		if (mask && !(com->mask & mask)) {
			x++;
			continue;
		}
		if (plugfilt != -2 && com->plugin != plugfilt) {
			x++;
			continue;
		}

		comMutex.Release();
		return *x;
	}

	comMutex.Release();
	return NULL;
}

COMMAND * FindCommand(const char * command) {
	AutoMutex(comMutex);
	for (commandList::const_iterator x = commands.begin(); x != commands.end(); x++) {
		if (!stricmp(command, (*x)->command)) {
			return *x;
		}
	}
	return NULL;
}

COMMAND * RegisterCommand2(int pluginnum, const char * command, const COMMAND_ACL * acl, uint32 mask, const char * action, const char * desc) {
#if !defined(IRCBOT_ENABLE_IRC)
	if (!(mask & (CM_ALLOW_CONSOLE_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE))) {
		ib_printf("WARNING! RegisterCommand: '%s' has no console access!\n", command);
	}
#endif
	AutoMutex(comMutex);
	COMMAND * pCmd = FindCommand(command);
	if (pCmd) {
		zfree((void *)pCmd->desc);
		if (pCmd->proc_type == COMMAND_ACTION) {
			zfree((void *)pCmd->action);
		}
		pCmd->desc = zstrdup(desc);
		pCmd->action = zstrdup(action);
		pCmd->acl = *acl;
		pCmd->mask = mask;
		pCmd->plugin = pluginnum;
		pCmd->proc_type = COMMAND_ACTION;
		return pCmd;
	} else {
		COMMAND * Scan = (COMMAND *)zmalloc(sizeof(COMMAND));
		memset(Scan, 0, sizeof(COMMAND));

		Scan->command = zstrdup(command);
		Scan->desc = zstrdup(desc);
		Scan->action = zstrdup(action);
		Scan->acl = *acl;
		Scan->mask = mask;
		Scan->plugin = pluginnum;
		Scan->proc_type = COMMAND_ACTION;

		commands.push_back(Scan);
		return Scan;
	}
	return NULL;
}

COMMAND * RegisterCommand2(int pluginnum, const char * command, const COMMAND_ACL * acl, uint32 mask, CommandProcType proc, const char * desc) {
	//ib_printf("RegisterCommand(%s,%d,%X)\n", command, minlevel, proc);
#if !defined(IRCBOT_ENABLE_IRC)
	if (!(mask & (CM_ALLOW_CONSOLE_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE))) {
		ib_printf("WARNING! RegisterCommand: '%s' has no console access!\n", command);
	}
#endif
	AutoMutex(comMutex);
	COMMAND * pCmd = FindCommand(command);
	if (pCmd) {
		zfree((void *)pCmd->command);
		zfree((void *)pCmd->desc);
		if (pCmd->proc_type == COMMAND_ACTION) {
			zfree((void *)pCmd->action);
		}
		pCmd->command = zstrdup(command);
		pCmd->desc = zstrdup(desc);
		pCmd->proc = proc;
		pCmd->acl = *acl;
		pCmd->mask = mask;
		pCmd->plugin = pluginnum;
		pCmd->proc_type = COMMAND_PROC;
		return pCmd;
	} else {
		COMMAND * Scan = (COMMAND *)zmalloc(sizeof(COMMAND));
		memset(Scan, 0, sizeof(COMMAND));

		Scan->command = zstrdup(command);
		Scan->desc = zstrdup(desc);
		Scan->proc = proc;
		Scan->acl = *acl;
		Scan->mask = mask;
		Scan->proc_type = COMMAND_PROC;
		Scan->plugin = pluginnum;

		commands.push_back(Scan);
		return Scan;
	}
	return NULL;
}

COMMAND_ACL * FillACL(COMMAND_ACL * acl, uint32 flags_have_all, uint32 flags_have_any, uint32 flags_not) {
	memset(acl, 0, sizeof(COMMAND_ACL));
	acl->flags_all = flags_have_all;
	acl->flags_any = flags_have_any;
	acl->flags_not = flags_not;
	return acl;
}
COMMAND_ACL MakeACL(uint32 flags_have_all, uint32 flags_have_any, uint32 flags_not) {
	COMMAND_ACL acl;
	memset(&acl, 0, sizeof(acl));
	acl.flags_all = flags_have_all;
	acl.flags_any = flags_have_any;
	acl.flags_not = flags_not;
	return acl;
}

void UnregisterCommand(COMMAND *  Scan) {
	comMutex.Lock();
	if (!IsValidCommand(Scan)) {
		comMutex.Release();
		return;
	}

	if (Scan->ref_cnt != 0) {
		ib_printf("WARNING: UnregisterCommand '%s' with ref_cnt: %u\n", Scan->command, Scan->ref_cnt);
		while (1) {
			ib_printf("Waiting for command '%s' to be dereferenced...\n", Scan->command);
			comMutex.Release();
			safe_sleep(1);
			comMutex.Lock();
			if (!IsValidCommand(Scan)) {
				comMutex.Release();
				return;
			}
			if (Scan->ref_cnt == 0) {
				break;
			}
		}
	}
	for (commandList::iterator x = commands.begin(); x != commands.end(); x++) {
		if (*x == Scan) {
			commands.erase(x);
			break;
		}
	}
	comMutex.Release();

	zfree((void *)Scan->command);
	zfree((void *)Scan->desc);
	if (Scan->proc_type == COMMAND_ACTION) {
		zfree((void *)Scan->action);
	}
	zfree(Scan);
}

void UnregisterCommand(const char * command) {
	COMMAND * Scan = FindCommand(command);
	if (Scan) {
		UnregisterCommand(Scan);
	}
}

void UnregisterCommandsByPlugin(int plugnum) {
	comMutex.Lock();
	for (commandList::iterator x = commands.begin(); x != commands.end();) {
		if ((*x)->plugin == plugnum) {
			UnregisterCommand(*x);
			x = commands.begin();
		} else {
			x++;
		}
	}
	comMutex.Release();
}

void UnregisterCommands() {
	commandList::iterator x = commands.begin();
	while (x != commands.end()) {
		UnregisterCommand(*x);
		x = commands.begin();
	}
}

void UnregisterCommandsWithMask(uint32 mask) {
	comMutex.Lock();
	for (commandList::iterator x = commands.begin(); x != commands.end();) {
		if ((*x)->mask & mask) {
			UnregisterCommand(*x);
			x = commands.begin();
		} else {
			x++;
		}
	}
	comMutex.Release();
}
