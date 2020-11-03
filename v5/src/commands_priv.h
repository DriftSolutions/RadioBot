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


extern Titus_Mutex comMutex;
#if !defined(OLD_COMTRACK)
typedef std::vector<COMMAND *> commandList;
extern commandList commands;
#else
extern COMMAND *fCmd, *lCmd;
#endif

struct HOOKS {
	HOOKS *Prev,*Next;
	const char * command;
	CommandProcType proc;
};

extern HOOKS *fHook, *lHook;

struct ASYNC_PROC {
	COMMAND * com;
	char * parms;
	USER_PRESENCE * ut;
	uint32 type;
};

typedef std::vector<ASYNC_PROC *> execList;
extern execList execQueue;

struct COMMAND_THREAD {
	bool running;
	bool shutdown;
	TT_THREAD_INFO * tt;
	ASYNC_PROC * exec;
};

bool IsValidCommand(COMMAND * com);
bool command_is_allowed(COMMAND * com, uint32 flags);

#define NUM_COM_THREADS 2
extern COMMAND_THREAD * com_threads;

