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

/*********************************************/
/*      Copyright 2004+ Drift Solutions      */
/*********************************************/

#ifndef __INCLUDE_IRCBOT_SENDQ_H__
#define __INCLUDE_IRCBOT_SENDQ_H__
#if defined(IRCBOT_ENABLE_IRC)

int bSend(T_SOCKET * sock, const char * buf, int32 len=0, uint8 priority=PRIORITY_DEFAULT, uint32 delay=0);
void SendQ_ClearSockEntries(T_SOCKET * sock);
uint32 SendQ_CountSockEntries(T_SOCKET * sock);
bool SendQ_HaveSockEntries(T_SOCKET * sock);

THREADTYPE SendQ_Thread(void * lpData);

#define SENDQ_HISTORY_LENGTH 10

struct SENDQ_STATS {
	uint64 bytesSent;
	uint64 bytesSentIRC;
	uint32 avgBps;
	uint32 queued_items;
	/*
	uint32 history_1[SENDQ_HISTORY_LENGTH];
	uint32 history_30[SENDQ_HISTORY_LENGTH];
	uint32 history_60[SENDQ_HISTORY_LENGTH];
	*/
};

SENDQ_STATS * SendQ_GetStats();

#endif // defined(IRCBOT_ENABLE_IRC)
#endif // __INCLUDE_IRCBOT_SENDQ_H__
