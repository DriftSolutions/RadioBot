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
#include "chanadminapi.h"

extern BOTAPI_DEF * api;
extern int pluginnum;

struct CHANADMIN_CONFIG {
	bool ProtectOps;
	bool BanOnKick;
	int BanExpiration;
};
extern CHANADMIN_CONFIG chanadmin_config;

sql_rows GetTable(const char * query, char **errmsg);

// Auto Modes
void handle_onjoin_automodes(USER_PRESENCE * ui);
int AutoMode_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

// Bans Module
bool handle_onjoin_bans(USER_PRESENCE * ui);
int Bans_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

void del_expired_bans();
bool add_ban(int netno, const char * chan, const char * hostmask, const char * set_by, int64 time_expire, const char * reason=NULL);
bool del_ban(int netno, const char * chan, const char * hostmask);
bool do_kick(int netno, const char * chan, const char * nick, const char * reason=NULL);
bool do_ban(int netno, const char * chan, const char * hostmask);
bool do_unban(int netno, const char * chan, const char * hostmask);

// Ignores Module
int Ignores_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

void del_expired_ignores();
bool is_ignored(USER_PRESENCE * ui);
bool add_ignore(const char * hostmask, const char * set_by, int64 time_expire, const char * reason=NULL);
bool del_ignore(const char * hostmask);

// Seen Module
void update_channel_seen(USER_PRESENCE * ui);
int Seen_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);
