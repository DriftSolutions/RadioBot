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

#ifndef __INCLUDE_IRCBOT_REQUESTS_H__
#define __INCLUDE_IRCBOT_REQUESTS_H__
#if defined(IRCBOT_ENABLE_SS)

enum DJNAME_MODES {
	DJNAME_MODE_STANDARD,
	DJNAME_MODE_NICK,
	DJNAME_MODE_USERNAME
};

bool DeliverRequest(USER_PRESENCE * from, const char * req, char * reply, size_t replySize);
bool DeliverRequest(USER_PRESENCE * from, FIND_RESULT * result, char * reply, size_t replySize);
extern Titus_Mutex requestMutex;
char * GetRequestDJNick();
void RequestsOff();
void RequestsOn(USER_PRESENCE * user, REQUEST_MODES mode, REQUEST_INTERFACE * iface=NULL);
void RequestsOn(REQUEST_INTERFACE * iface);

string GenFindID(USER_PRESENCE * ut);
void SaveFindResult(const char * id, FIND_RESULTS * res);
FIND_RESULTS * GetFindResult(const char * id);
void ExpireFindResults(bool all=false, int plugin=-1);

#endif // defined(IRCBOT_ENABLE_SS)
#endif // __INCLUDE_IRCBOT_REQUESTS_H__

