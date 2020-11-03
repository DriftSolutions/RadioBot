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


#ifndef __INCLUDE_IRCBOT_USERS_H__
#define __INCLUDE_IRCBOT_USERS_H__

IBM_USEREXTENDED * UserToExtended(USER * User, IBM_USEREXTENDED * UserEx);
USER * GetUserByNick(const char * nick);
USER * GetUser(const char * hostmask);
void UpdateUserSeen(USER_PRESENCE * up);

extern const char valid_nick_chars[];
bool IsUsernameLegal(const char * nick, char * errBuf=NULL, int errSize=0);
void FixUsername(char * nick);

uint32 GetUserCount();
//const char * GetUserName(const char * hostmask);
bool IsValidUserName(const char * nick);
uint32 GetUserFlags(const char * hostmask);
//const char * GetUserPass(const char * hostmask);

USER * AddUser(const char * nick, const char * pass, uint32 flags, bool temp=false);
void DelUser(USER * user);
void SetUserFlags(USER * user, uint32 flags);
void AddUserFlags(USER * user, uint32 flags);
void DelUserFlags(USER * user, uint32 flags);
void SetUserPass(USER * user, const char * pass);
bool AddHostmask(USER * user, const char * hostmask);
bool ClearHostmask(USER * user, const char * hostmask);
bool ClearAllHostmasks(USER * user);

bool uflag_have_any_of(uint32 flags, uint32 wanted);
int uflag_compare(uint32 flags1, uint32 flags2);
uint32 GetDefaultFlags();
uint32 GetLevelFlags(int level);
//void AddToLevelFlags(int level, uint32  flags);
void SetLevelFlags(int level, uint32  flags);
void uflag_to_string(uint32 flags, char * buf, size_t bufSize);
uint32 string_to_uflag(const char * buf, uint32 cur=0);
uint32 letter_to_uflag(char c);

typedef bool (*EnumUsersCallback)(const char * nick, void * ptr);
void EnumUsers(EnumUsersCallback callback, void * ptr);

typedef bool (*EnumHostmasksCallback)(const char * nick, const char * hostmask, void * ptr);
void EnumHostmasks(EnumHostmasksCallback callback, void * ptr);

void EnumUsersByLastHostmask(EnumUsersByLastHostmaskCallback callback, const char * hostmask, void * ptr);

/* Create tables, wipe any temporary users from DB, etc. */
void InitDatabase();
int GetDatabaseVersion();

/* Import v3 users if ircbot.users exists... */
bool LoadOldUsers();
#endif
