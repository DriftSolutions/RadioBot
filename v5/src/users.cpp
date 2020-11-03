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

#if defined(__EDITUSERS__)
#include "../../editusers/editusers.h"
#define IRCBOT_NAME "RadioBot"
#else
#include "ircbot.h"
#endif

Titus_Mutex userMutex;
typedef std::vector<USER *> userListType;
userListType userList;
int db_version = 0;

/**
 * Adds 1 to the ref count of a USER structure
 */
#if !defined(DEBUG)
void user_add_ref(USER * u) {
#else
void user_add_ref(USER * u, const char * fn, int line) {
	ib_printf("User_AddRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	userMutex.Lock();
	u->ref_cnt++;
	userMutex.Release();
}

/**
 * Decreases the ref count of a USER structure and frees it if it reaches zero
 */
#if !defined(DEBUG)
void user_del_ref(USER * u) {
#else
void user_del_ref(USER * u, const char * fn, int line) {
	ib_printf("User_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	userMutex.Lock();
	u->ref_cnt--;
	#if defined(DEBUG)
	ib_printf("User_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
	#endif
	if (u->ref_cnt == 0) {
		for (userListType::iterator x = userList.begin(); x != userList.end(); x++) {
			if (*x == u) {
				userList.erase(x);
				break;
			}
		}
		zfree(u);
	}
	userMutex.Release();
}

/*
void delete_stale_users(bool all) {
	AutoMutex(userMutex);
	for (userListType::iterator x = userList.begin(); x != userList.end();) {
		if (all || (*x)->ref_cnt == 0) {
			USER * u = *x;
			userList.erase(x);
			zfree(u);
			x = userList.begin();
		} else {
			x++;
		}
	}
}
*/

/**
 * Allocates and zeroes out a new USER structure and does the initial RefUser.
 */
USER * alloc_user() {
	USER * ret = znew(USER);
	memset(ret, 0, sizeof(USER));

	ret->ref = user_add_ref;
	ret->unref = user_del_ref;
	RefUser(ret);

	return ret;
}

/**
 * Takes a USER structure and fills in an IBM_USEREXTENDED structure with it's info.
 * @param User ptr to a valid USER structure
 * @param UserEx ptr to a valid IBM_USEREXTENDED structure. This function will zero the memory in the struct so you don't have to.
 */
IBM_USEREXTENDED * UserToExtended(USER * User, IBM_USEREXTENDED * UserEx) {
	memset(UserEx, 0, sizeof(IBM_USEREXTENDED));
	sstrcpy(UserEx->nick, User->Nick);
	sstrcpy(UserEx->pass, User->Pass);
	UserEx->flags = User->Flags;
	UserEx->numhostmasks = User->NumHostmasks;
	for (int i=0; i < MAX_HOSTMASKS; i++) {
		sstrcpy(UserEx->hostmasks[i], User->Hostmasks[i]);
	}
	return UserEx;
}

/**
 * Takes a USER structure and fills in an IBM_USERFULL structure with it's info.
 * @param User ptr to a valid USER structure
 * @param UserEx ptr to a valid IBM_USERFULL structure. This function will zero the memory in the struct so you don't have to.
 */
IBM_USERFULL * UserToFull(USER * User, IBM_USERFULL * UserEx) {
	memset(UserEx, 0, sizeof(IBM_USERFULL));
	sstrcpy(UserEx->nick, User->Nick);
	sstrcpy(UserEx->pass, User->Pass);
	UserEx->flags = User->Flags;
	return UserEx;
}

/**
 * Grabs a user's info and creates a USER structure. Pushes USER into the user list.
 * @param nick a string containing the username to read
 */
USER * GetUserFromDB(const char * nick) {
	userMutex.Lock();
	USER * ret = NULL;
	std::ostringstream sstr;
	sstr << "SELECT * FROM Users WHERE Nick='" << nick << "'";
	sql_rows res = GetTable(sstr.str().c_str());
	if (res.size() == 1) {
		ret = alloc_user();
		sql_row row = res.begin()->second;

		sstrcpy(ret->Nick, row["Nick"].c_str());
		sstrcpy(ret->Pass, row["Pass"].c_str());
		sstrcpy(ret->LastHostmask, row["LastHostmask"].c_str());
		int Level = atoi(row["Level"].c_str());
		if (Level > 0) {
			ret->Flags = GetLevelFlags(Level);
			std::ostringstream sstr2;
			sstr2 << "UPDATE Users SET Level=0, Flags=" << ret->Flags << " WHERE Nick='" << nick << "'";
			Query(sstr2.str().c_str(), NULL, NULL, NULL);
		} else {
			ret->Flags = atoul(row["Flags"].c_str());
		}
		ret->Temporary = atoi(row["Temp"].c_str());
		ret->Created = atoi64(row["Created"].c_str());
		ret->Seen = atoi64(row["Seen"].c_str());

		std::ostringstream sstr2;
		sstr2 << "SELECT * FROM UserHostmasks WHERE Nick='" << nick << "' LIMIT " << MAX_HOSTMASKS;
		res = GetTable(sstr2.str().c_str());
		ret->NumHostmasks = res.size();
		if (ret->NumHostmasks > 0) {
			int i = 0;
			for (sql_rows::const_iterator x = res.begin(); x != res.end(); x++) {
				row = x->second;
				sstrcpy(ret->Hostmasks[i], row["Hostmask"].c_str());
				i++;
			}
		}

		userList.push_back(ret);
	}
	userMutex.Release();
	return ret;
}

/**
 * Creates a new user account in the database and returns a USER structure. Returns ptr to USER on success, NULL on failure.
 * @param nick the username to use
 * @param pass the user's password
 * @param level the user's access level (1-5)
 * @param temp if true, mark the user as temporary so they will not be reloaded the next time the bot is loaded
 */
USER * AddUser(const char * nick, const char * pass, uint32 flags, bool temp) {
	userMutex.Lock();

	char buf[256];
	if (!IsUsernameLegal(nick, buf, sizeof(buf))) {
		ib_printf(_("%s: AddUser(): Illegal username '%s': %s\n"), IRCBOT_NAME, nick, buf);
		userMutex.Release();
		return NULL;
	}

	std::ostringstream sstr;
	char * p = MPrintf("%q", pass);
	sstr << "INSERT INTO Users (Nick,Pass,Flags,Created,Temp) VALUES ('" << nick << "','" << p << "', " << flags << ", " << time(NULL) << ", " << temp << ")";
	Free(p);
	if (Query(sstr.str().c_str()) != SQLITE_OK) {
		userMutex.Release();
		return NULL;
	}

	USER *Scan = alloc_user();

	sstrcpy(Scan->Nick, nick);
	sstrcpy(Scan->Pass, pass);
	Scan->Flags = flags;
	Scan->Temporary = temp;
	Scan->Created = time(NULL);

	userList.push_back(Scan);
	userMutex.Release();
	return Scan;
}

/**
 * Delete's a user from the database. Also calls UnrefUser on the structure.
 * @param user a ptr to a valid user structure
 */
void DelUser(USER * user) {
	userMutex.Lock();

#if !defined(__EDITUSERS__)
	IBM_USERFULL ui;
	UserToFull(user, &ui);
	SendMessage(-1, IB_DEL_USER, (char *)&ui, sizeof(ui));
#endif

	std::ostringstream sstr;
	sstr << "DELETE FROM Users WHERE Nick='" << user->Nick << "'";
	Query(sstr.str().c_str());
	std::ostringstream sstr2;
	sstr2 << "DELETE FROM UserHostmasks WHERE Nick='" << user->Nick << "'";
	Query(sstr2.str().c_str());

#if !defined(__EDITUSERS__)
	// The next lines are in case anyone has a reference to the user besides the calling function
	user->Pass[0] = 0;
	user->NumHostmasks = 0;
	user->Flags = config.base.default_flags;
	user->Temporary = true;
#endif

	UnrefUser(user);
	userMutex.Release();
}

/**
 * Adds a hostmask to a user (as long as they have less than MAX_HOSTMASKS hostmasks)<br>
 * Returns true if the hostmask was added, false otherwise.
 * @param user a ptr to a valid user structure
 * @param hostmask a string containing a hostmask
 */
bool AddHostmask(USER * user, const char * hostmask) {
	userMutex.Lock();
	ClearHostmask(user, hostmask);

#if !defined(__EDITUSERS__)
	IBM_USERFULL ui;
	UserToFull(user, &ui);
	sstrcpy(ui.hostmask, hostmask);
	SendMessage(-1, IB_ADD_HOSTMASK, (char *)&ui, sizeof(ui));
#endif

	if (user->NumHostmasks < MAX_HOSTMASKS) {
		sstrcpy(user->Hostmasks[user->NumHostmasks], hostmask);
		std::ostringstream sstr;
		char * p = MPrintf("%q", hostmask);
		sstr << "INSERT INTO UserHostmasks (Nick,Hostmask) VALUES ('" << user->Nick << "','" << p << "')";
		Free(p);
		Query(sstr.str().c_str());
		user->NumHostmasks++;
		userMutex.Release();
		return true;
	}
	userMutex.Release();
	return false;
}

/**
 * Removes a hostmask from a user (if user has said hostmask)<br>
 * Returns true if the hostmask was removed, false otherwise.<br>
 * A return of false isn't necessarily bad, it means the user didn't have the hostmask in their USER record.
 * @param user a ptr to a valid user structure
 * @param hostmask a string containing a hostmask
 */
bool ClearHostmask(USER * user, const char * hostmask) {
	userMutex.Lock();

#if !defined(__EDITUSERS__)
	IBM_USERFULL ui;
	UserToFull(user, &ui);
	sstrcpy(ui.hostmask, hostmask);
	SendMessage(-1, IB_DEL_HOSTMASK, (char *)&ui, sizeof(ui));
#endif

	bool matched = false;
	for (int i=0; i < user->NumHostmasks; i++) {
		if (!stricmp(user->Hostmasks[i], hostmask)) {
			matched = true;
			if (i == 0) {
				memcpy(&user->Hostmasks[0], &user->Hostmasks[1], sizeof(user->Hostmasks[0])*(MAX_HOSTMASKS-1));
				memset(&user->Hostmasks[MAX_HOSTMASKS - 1], 0, sizeof(user->Hostmasks[0]));
			} else if (i == (MAX_HOSTMASKS-1)) {
				memset(&user->Hostmasks[MAX_HOSTMASKS - 1], 0, sizeof(user->Hostmasks[0]));
			} else {
				memcpy(&user->Hostmasks[i], &user->Hostmasks[i+1], sizeof(user->Hostmasks[0])*(MAX_HOSTMASKS-i));
				memset(&user->Hostmasks[MAX_HOSTMASKS - 1], 0, sizeof(user->Hostmasks[0]));
			}
			user->NumHostmasks--;

			std::ostringstream sstr;
			char * p = MPrintf("%q", hostmask);
			sstr << "DELETE FROM UserHostmasks WHERE Nick='" << user->Nick << "' AND Hostmask='" << p << "'";
			Free(p);
			Query(sstr.str().c_str());
		}
	}
	userMutex.Release();
	return matched;
}

/**
 * Removes all of a user's hostmasks<br>
 * Returns true if the hostmask(s) were removed, false otherwise.
 * @param user a ptr to a valid user structure
 */
bool ClearAllHostmasks(USER * user) {
	userMutex.Lock();
	user->NumHostmasks = 0;
	memset(&user->Hostmasks, 0, sizeof(user->Hostmasks));

#if !defined(__EDITUSERS__)
	IBM_USERFULL ui;
	UserToFull(user, &ui);
	SendMessage(-1, IB_DEL_HOSTMASKS, (char *)&ui, sizeof(ui));
#endif

	std::ostringstream sstr;
	sstr << "DELETE FROM UserHostmasks WHERE Nick='" << user->Nick << "'";
	Query(sstr.str().c_str());

	userMutex.Release();
	return true;
}

/**
 * Changes a user's level.
 * @param user a ptr to a valid user structure
 * @param level the user's new level (1-5)
 */
void SetUserFlags(USER * user, uint32 flags) {
	userMutex.Lock();
	if (user->Flags & UFLAG_MASTER) {
		flags |= UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ|UFLAG_DIE;
	}
	user->Flags = flags;

#if !defined(__EDITUSERS__)
	IBM_USERFULL ui;
	memset(&ui, 0, sizeof(ui));
	sstrcpy(ui.nick, user->Nick);
	sstrcpy(ui.pass, user->Pass);
	ui.flags = flags;
	SendMessage(-1, IB_CHANGE_FLAGS, (char *)&ui, sizeof(ui));
#endif

	std::ostringstream sstr;
	sstr << "UPDATE Users SET Flags=" << flags << " WHERE Nick='" << user->Nick << "'";
	Query(sstr.str().c_str());
	userMutex.Release();
}
void AddUserFlags(USER * user, uint32 flags) {
	userMutex.Lock();
	SetUserFlags(user, user->Flags|flags);
	userMutex.Release();
}
void DelUserFlags(USER * user, uint32 flags) {
	userMutex.Lock();
	SetUserFlags(user, user->Flags & ~flags);
	userMutex.Release();
}

/**
 * Changes a user's password.
 * @param user a ptr to a valid user structure
 * @param pass the user's new password
 */
void SetUserPass(USER * user, const char * pass) {
	userMutex.Lock();

#if !defined(__EDITUSERS__)
	IBM_USERFULL ui;
	memset(&ui, 0, sizeof(ui));
	sstrcpy(ui.nick, user->Nick);
	sstrcpy(ui.pass, pass);
	ui.flags = user->Flags;
	SendMessage(-1, IB_CHANGE_PASS, (char *)&ui, sizeof(ui));
#endif

	sstrcpy(user->Pass, pass);
	std::ostringstream sstr;
	char * p = MPrintf("%q", pass);
	sstr << "UPDATE Users SET Pass='" << p << "' WHERE Nick='" << user->Nick << "'";
	Free(p);
	Query(sstr.str().c_str());
	userMutex.Release();
}

/**
 * Updates are user's last seen time.
 * @param user a ptr to a valid user structure
 */
void UpdateUserSeen(USER_PRESENCE * up) {
	if (up == NULL || up->User == NULL) {
		return;
	}
	USER * user = up->User;
	userMutex.Lock();
	user->Seen = time(NULL);
	if (strcmp(user->LastHostmask, up->Hostmask)) { sstrcpy(user->LastHostmask, up->Hostmask); }
	std::ostringstream sstr;
	char * p = MPrintf("%q", up->Hostmask);
	sstr << "UPDATE Users SET Seen='" << user->Seen << "',LastHostmask='" << p << "' WHERE Nick='" << user->Nick << "'";
	Free(p);
	Query(sstr.str().c_str());
	userMutex.Release();
}

bool uflag_have_any_of(uint32 flags, uint32 wanted) {
	return (flags & wanted) ? true:false;
}

uint32 default_flags[5] = {
	UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ|UFLAG_DIE|UFLAG_BASIC_SOURCE|UFLAG_ADVANCED_SOURCE|UFLAG_REMOTE|UFLAG_REQUEST|UFLAG_CHANADMIN|UFLAG_SKYPE|UFLAG_DCC_ADMIN,
	UFLAG_OP|UFLAG_HOP|UFLAG_DJ|UFLAG_BASIC_SOURCE|UFLAG_ADVANCED_SOURCE|UFLAG_REMOTE|UFLAG_REQUEST|UFLAG_CHANADMIN|UFLAG_SKYPE|UFLAG_DCC_ADMIN,
	UFLAG_HOP|UFLAG_DJ|UFLAG_BASIC_SOURCE|UFLAG_ADVANCED_SOURCE|UFLAG_REMOTE|UFLAG_REQUEST,
	UFLAG_DJ|UFLAG_BASIC_SOURCE|UFLAG_REMOTE|UFLAG_REQUEST,
	UFLAG_REQUEST
};
uint32 GetLevelFlags(int level) {
	return default_flags[level-1];
}
void SetLevelFlags(int level, uint32 flags) {
	default_flags[level-1] = flags;
}
void uflag_to_string(uint32 flags, char * buf, size_t bufSize) {
	char *p = buf;
	*p++ = '+';
	char ccur = 'a';
	uint32 icur = 1;
	while (icur <= 0x02000000) {
		if (icur & flags) {
			*p++ = ccur;
		}
		icur <<= 1;
		ccur++;
	}
	ccur = 'A';
	while (icur != 0) {
		if (icur & flags) {
			*p++ = ccur;
		}
		icur <<= 1;
		ccur++;
	}
	*p = 0;
}

int uflag_compare(uint32 flags1, uint32 flags2) {
	if (flags1 & UFLAG_MASTER & flags2) {
		return 0;//equal rank
	}
	if ((flags1 & UFLAG_MASTER) && !(flags2 & UFLAG_MASTER)) {
		return 1;//user 1 is higher rank
	}
	if (!(flags1 & UFLAG_MASTER) && (flags2 & UFLAG_MASTER)) {
		return -1;//user 1 is lower rank
	}
	if (flags1 & UFLAG_OP & flags2) {
		return 0;//equal rank
	}
	if ((flags1 & UFLAG_OP) && !(flags2 & UFLAG_OP)) {
		return 1;//user 1 is higher rank
	}
	if (!(flags1 & UFLAG_OP) && (flags2 & UFLAG_OP)) {
		return -1;//user 1 is lower rank
	}
	if (flags1 & UFLAG_HOP & flags2) {
		return 0;//equal rank
	}
	if ((flags1 & UFLAG_HOP) && !(flags2 & UFLAG_HOP)) {
		return 1;//user 1 is higher rank
	}
	if (!(flags1 & UFLAG_HOP) && (flags2 & UFLAG_HOP)) {
		return -1;//user 1 is lower rank
	}
	if (flags1 & UFLAG_DJ & flags2) {
		return 0;//equal rank
	}
	if ((flags1 & UFLAG_DJ) && !(flags2 & UFLAG_DJ)) {
		return 1;//user 1 is higher rank
	}
	if (!(flags1 & UFLAG_DJ) && (flags2 & UFLAG_DJ)) {
		return -1;//user 1 is lower rank
	}
	return -1; //user 1 is lower rank
}

uint32 letter_to_uflag(char c) {
	if (c >= 'a' && c <= 'z') {
		return UFLAG_a<<(c-'a');
	}
	if (c >= 'A' && c <= 'F') {
		return UFLAG_A<<(c-'A');
	}
	return 0;
}
uint32 string_to_uflag(const char * buf, uint32 cur) {
	uint32 ret = cur;
	int mode = -1;
	const char * p = buf;
	while (*p) {
		if (*p == '=') {
			mode = 0;
			ret = 0;
		} else if (*p == '+') {
			mode = 0;
		} else if (*p == '-') {
			mode = 1;
		} else {
			uint32 cm = letter_to_uflag(*p);
			if (cm) {
				switch (mode) {
					case 0:
						ret |= cm;
						break;
					case 1:
						ret &= ~cm;
						break;
				}
			} else {
				ib_printf("%s: Unrecognized mode char: %c\n", IRCBOT_NAME, *p);
			}
		}
		p++;
	}
	return ret;
}

/**
Get's a USER handle by nickname. If the return is non-NULL it is a valid USER handle that has been RefUser'ed, make sure you UnrefUser it when you are done.<br>
Search order:<br>
1. The function will check the existing USER handles for a match.<br>
2. Send out an IB_GETUSERINFO to check with any auth plugins.<br>
3. Check the IRCBot user database.<br>
4. Return NULL on failure.
@param nick a string holding a username
 */
USER * GetUserByNick(const char * nick) {
	userMutex.Lock();
	for (userListType::const_iterator x = userList.begin(); x != userList.end(); x++) {
		if (!stricmp((*x)->Nick, nick)) {
			RefUser(*x);
			userMutex.Release();
			return *x;
		}
	}

#if !defined(__EDITUSERS__)
	IBM_GETUSERINFO ui;
	memset(&ui,0,sizeof(ui));
	sstrcpy(ui.ui.nick,nick);
	if (GetAPI()->SendMessage(-1,IB_GETUSERINFO,(char *)&ui,sizeof(ui))) {
		if (ui.type == IBM_UT_FULL) {
			if (ui.ui.flags != 0) {
				USER * user = GetUserFromDB(ui.ui.nick);
				if (user == NULL) {
					user = AddUser(ui.ui.nick, ui.ui.pass, ui.ui.flags, true);
				} else {
					if (strlen(ui.ui.pass)) {
						SetUserPass(user, ui.ui.pass);
					}
					SetUserFlags(user, ui.ui.flags);
				}
				char buf[128];
				snprintf(buf, sizeof(buf), "*%s*!*@*", nick);
				AddHostmask(user, buf);
				userMutex.Release();
				return user;
			}
		} else if (ui.type == IBM_UT_EXTENDED) {
			if (ui.ue.flags != 0) {
				USER * user = GetUserFromDB(ui.ue.nick);
				if (user == NULL) {
					user = AddUser(ui.ue.nick, ui.ue.pass, ui.ue.flags, true);
				} else {
					if (strlen(ui.ue.pass)) {
						SetUserPass(user, ui.ue.pass);
					}
					SetUserFlags(user, ui.ue.flags);
				}
				for (int i=0; i < ui.ue.numhostmasks; i++) {
					AddHostmask(user, ui.ue.hostmasks[i]);
				}
				userMutex.Release();
				return user;
			}
		}
	}
#endif

	USER * ret = GetUserFromDB(nick);
	userMutex.Release();
	return ret;
}

/**
Get's a USER handle by hostmask. If the return is non-NULL it is a valid USER handle that has been RefUser'ed, make sure you UnrefUser it when you are done.<br>
Search order:<br>
1. The function will check the existing USER handles for a match.<br>
2. Send out an IB_GETUSERINFO to check with any auth plugins.<br>
3. Check the IRCBot user database.<br>
4. Return NULL on failure.
@param hostmask a string holding a hostmask
WARNING: Passwords are not checked in this function, it is simply passed to the auth plugins in case they can use it!!!
 */
USER * GetUser(const char * hostmask) {
	userMutex.Lock();
	for (userListType::const_iterator x = userList.begin(); x != userList.end(); x++) {
		USER * Scan = *x;
		for (int i=0; i < Scan->NumHostmasks; i++) {
			if (wildicmp(Scan->Hostmasks[i], hostmask)) {
				RefUser(*x);
				if (strcmp((*x)->LastHostmask, hostmask)) { sstrcpy((*x)->LastHostmask, hostmask); }
				userMutex.Release();
				return Scan;
			}
		}
	}

#if !defined(__EDITUSERS__)
	char * nick = zstrdup(hostmask);
	char * nick_p = strchr(nick, '!');
	if (nick_p) {
		*nick_p = 0;
	}

	IBM_GETUSERINFO ui;
	memset(&ui,0,sizeof(ui));
	sstrcpy(ui.ui.nick, nick);
	sstrcpy(ui.ui.hostmask, hostmask);
	//if (pass) { strcpy(ui.ui.pass, pass); }
	if (GetAPI()->SendMessage(-1,IB_GETUSERINFO,(char *)&ui,sizeof(ui))) {
		if (ui.type == IBM_UT_FULL) {
			if (ui.ui.flags != 0) {
				USER * user = GetUserFromDB(ui.ui.nick);
				if (user == NULL) {
					user = AddUser(ui.ui.nick, ui.ui.pass, ui.ui.flags, true);
				} else {
					if (strlen(ui.ui.pass)) {
						SetUserPass(user, ui.ui.pass);
					}
					SetUserFlags(user, ui.ui.flags);
				}
				if (user) {
					char buf[128];
					sprintf(buf, "*%s*!*@*", nick);
					AddHostmask(user, buf);
					if (strcmp(user->LastHostmask, hostmask)) { sstrcpy(user->LastHostmask, hostmask); }
					userMutex.Release();
					zfree(nick);
					return user;
				}
			}
		} else if (ui.type == IBM_UT_EXTENDED) {
			if (ui.ue.flags != 0) {
				USER * user = GetUserFromDB(ui.ue.nick);
				if (user == NULL) {
					user = AddUser(ui.ue.nick, ui.ue.pass, ui.ue.flags, true);
				} else {
					if (strlen(ui.ue.pass)) {
						SetUserPass(user, ui.ue.pass);
					}
					SetUserFlags(user, ui.ue.flags);
				}
				if (user) {
					//user->Seen = ui.ue.seen;
					for (int i=0; i < ui.ue.numhostmasks; i++) {
						AddHostmask(user, ui.ue.hostmasks[i]);
					}
					if (strcmp(user->LastHostmask, hostmask)) { sstrcpy(user->LastHostmask, hostmask); }
					userMutex.Release();
					zfree(nick);
					return user;
				}
			}
		}
	}
	zfree(nick);
#endif

	USER * ret = NULL;

	std::ostringstream sstr;
	sstr << "SELECT * FROM UserHostmasks";
	sql_rows res = GetTable(sstr.str().c_str());
	for (sql_rows::const_iterator x = res.begin(); x != res.end(); x++) {
		sql_row row = x->second;
		if (wildicmp(row["Hostmask"].c_str(), hostmask)) {
			if ((ret = GetUserFromDB(row["Nick"].c_str())) != NULL) {
				break;
			}
		}
	}

	if (ret != NULL && strcmp(ret->LastHostmask, hostmask)) {
		sstrcpy(ret->LastHostmask, hostmask);
	}

	userMutex.Release();
	return ret;
}

/**
 * Checks to see if the user specified by nick exists.
 * @param nick a string holding a username
 */
bool IsValidUserName(const char * nick) {
	USER * Scan = GetUserByNick(nick);
	if (Scan) {
		UnrefUser(Scan);
		return true;
	}
	return false;
}

/**
 * Gets the access level of the user who has a matching hostmask.
 * @param hostmask a string holding a hostmask
 */
uint32 GetUserFlags(const char * hostmask) {
	USER * user = GetUser(hostmask);
	if (user) {
		int ret = user->Flags;
		UnrefUser(user);
		return ret;
	}
	return GetDefaultFlags();
}

/**
 * Gets the number of users in the IRCBot user database. Includes temporary users.
 */
uint32 GetUserCount() {
	sql_rows res = GetTable("SELECT COUNT(*) AS Count FROM Users");
	if (res.size() == 1) {
		sql_row row = res.begin()->second;
		return strtoul(row["Count"].c_str(), NULL, 10);
	}
	return 0;
}

/**
 * Makes sure user tables exist, wipes temporary users.
 */
void InitDatabase() {
	userMutex.Lock();
	//Query("DROP TABLE Users");
	//Query("DROP TABLE UserHostmasks");
	db_version = 1;
	Query("CREATE TABLE IF NOT EXISTS Version (Version INT UNSIGNED)");
	sql_rows res = GetTable("SELECT * FROM Version");
	if (res.size() > 0) {
		sql_row row = res.begin()->second;
		db_version = atoi(row["Version"].c_str());
	} else {
		Query("INSERT INTO Version (Version) VALUES (1)");
	}
	Query("CREATE TABLE IF NOT EXISTS Users (Nick TEXT UNIQUE COLLATE NOCASE, Pass TEXT COLLATE NOCASE, Level INT, Flags INT UNSIGNED, Created INT UNSIGNED, Seen INT UNSIGNED, Temp INT)");
	Query("CREATE TABLE IF NOT EXISTS UserHostmasks (Nick TEXT COLLATE NOCASE, Hostmask TEXT COLLATE NOCASE)");

	if (db_version < 2) {
		Query("ALTER TABLE Users ADD COLUMN LastHostmask TEXT COLLATE NOCASE");
		Query("UPDATE Version SET Version=2");
		db_version = 2;
	}
#if !defined(__EDITUSERS__)
	if (db_version < 3) {
		RebuildRatings();
		Query("UPDATE Version SET Version=3");
		db_version = 3;
	}
#endif

	std::ostringstream sstr;
	sstr << "SELECT * FROM Users WHERE Temp!=0";
	res = GetTable(sstr.str().c_str());
	for (sql_rows::const_iterator x = res.begin(); x != res.end(); x++) {
		sql_row row = x->second;
		std::ostringstream sstr2;
		sstr2 << "DELETE FROM Users WHERE Nick LIKE '" << row["Nick"] << "'";
		Query(sstr2.str().c_str());
		std::ostringstream sstr3;
		sstr3 << "DELETE FROM UserHostmasks WHERE Nick LIKE '" << row["Nick"] << "'";
		Query(sstr3.str().c_str());
	}

	userMutex.Release();
}

int GetDatabaseVersion() {
	AutoMutex(userMutex);
	return db_version;
}

/**
 * Enumerate all users in the IRCBot DB
 */
void EnumUsers(EnumUsersCallback callback, void * ptr) {
	if (callback == NULL) { return; }
	userMutex.Lock();

	std::ostringstream sstr;
	sstr << "SELECT * FROM Users ORDER BY Nick ASC";
	sql_rows res = GetTable(sstr.str().c_str());
	for (sql_rows::const_iterator x = res.begin(); x != res.end(); x++) {
		sql_row row = x->second;
		if (!callback(row["Nick"].c_str(), ptr)) {
			break;
		}
	}

	userMutex.Release();
}

/**
 * Enumerate all hostmasks in the IRCBot DB
 */
void EnumHostmasks(EnumHostmasksCallback callback, void * ptr) {
	if (callback == NULL) { return; }
	userMutex.Lock();

	std::ostringstream sstr;
	sstr << "SELECT * FROM UserHostmasks ORDER BY Nick ASC";
	sql_rows res = GetTable(sstr.str().c_str());
	for (sql_rows::const_iterator x = res.begin(); x != res.end(); x++) {
		sql_row row = x->second;
		if (!callback(row["Nick"].c_str(), row["Hostmask"].c_str(), ptr)) {
			break;
		}
	}

	userMutex.Release();
}


/**
Enumerates users with matching LastHostmask.
@param callback a function to call for each matching user
@param hostmask a string holding a hostmask
@param ptr a user-defined pointer passed to the callback function
 */
void EnumUsersByLastHostmask(EnumUsersByLastHostmaskCallback callback, const char * hostmask, void * ptr) {
	userMutex.Lock();

	USER * ret = NULL;

	std::ostringstream sstr;
	sstr << "SELECT Nick,LastHostmask FROM Users WHERE LastHostmask!=''";
	sql_rows res = GetTable(sstr.str().c_str());
	for (sql_rows::const_iterator x = res.begin(); x != res.end(); x++) {
		sql_row row = x->second;
		if (wildicmp(hostmask, row["LastHostmask"].c_str())) {
			if ((ret = GetUserByNick(row["Nick"].c_str())) != NULL) {
				if (!callback(ret, ptr)) {
					UnrefUser(ret);
					break;
				}
				UnrefUser(ret);
			}
		}
	}

	userMutex.Release();
}

const char valid_nick_chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";

/**
 * Checks to make sure username only has valid characters in it (see valid_nick_chars[])
 * @param nick a string containing a username
 */
bool IsUsernameLegal(const char * nick, char * errBuf, int errSize) {
	if (strspn(nick, valid_nick_chars) != strlen(nick)) {
		if (errBuf) { strlcpy(errBuf, _("Username has illegal characters"), errSize); }
		return false;
	}
	if (*nick >= '0' && *nick <= '9') {
		if (errBuf) { strlcpy(errBuf, _("Username starts with a number"), errSize); }
		return false;
	}
	return true;
}

/**
 * Removes any illegal characters from a username. (remember to check for an empty string when done in case all chars were illegal!)
 * @param nick a string containing a username
 */
void FixUsername(char * nick) {
	char * p = nick;
	while (*p) {
		if (strchr(valid_nick_chars, *p) == NULL) {
			//nope, not a good one
			memmove(p, p+1, strlen(p));
		} else if (p == nick && *p >= '0' && *p <= '9') {
			memmove(p, p+1, strlen(p));
		} else {
			p++;
		}
	}
}


// Old User Stuff

#pragma pack(1)
struct USERS_HEADER {
	unsigned long magic1,magic2;
	unsigned char version;
};
#pragma pack()

struct USERS_OLD {
	USERS_OLD *Prev,*Next;
	char Nick[128];
	char Pass[128];
	int32 Level;
	int32 NumHostmasks;
	char Hostmasks[MAX_HOSTMASKS][128];
	union {
		uint32 Flags;
		bool Temporary;// gotten from a remote source, won't be saved
	};
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
COMPILE_TIME_ASSERT(sizeof(USERS_OLD) == 2324)
#endif

/**
 * Loads old IRCBot v3 users into the DB, deletes ircbot.users when done.
 */
bool LoadOldUsers() {
	FILE * fp = fopen("ircbot.users", "rb");
	if (!fp) {
		return false;
	}

	USERS_HEADER head;
	if (fread(&head, sizeof(head), 1, fp) != 1) {
		ib_printf(_("Error: Error reading header in ircbot.users!\n"));
		fclose(fp);
		return false;
	}
	if (head.magic1 != 0x54465244 || head.magic2 != 0x52455355) {
		ib_printf(_("Error: Invalid magic in ircbot.users!\n"));
		fclose(fp);
		return false;
	}
	if (head.version > 0) {
		ib_printf(_("Error: Unknown version in ircbot.users!\n"));
		fclose(fp);
		return false;
	}

	USERS_OLD user;
	//int x = sizeof(USERS_OLD);
	while (fread(&user, sizeof(USERS_OLD), 1, fp) == 1) {
		ib_printf(_("Importing IRCBot v3 user: %s\n"), user.Nick);
		if (!IsUsernameLegal(user.Nick)) {
			char * orig = zstrdup(user.Nick);
			FixUsername(user.Nick);
			if (user.Nick[0] != 0) {
				ib_printf(_("%s: Altered username %s to %s\n"), IRCBOT_NAME, orig, user.Nick);
			} else {
				ib_printf(_("%s: Could not import user %s (all illegal characters in username)\n"), IRCBOT_NAME, orig);
			}
			zfree(orig);
		}
		if (user.Nick[0]) {
			USER * u = AddUser(user.Nick, user.Pass, GetLevelFlags(user.Level));
			if (u) {
				for (int i=0; i < user.NumHostmasks; i++) {
					AddHostmask(u, user.Hostmasks[i]);
				}
				UnrefUser(u);
			} else {
				ib_printf(_("Error creating user: %s\n"), user.Nick);
			}
		}
	}

	fclose(fp);
	remove("ircbot.users");
	return true;
}
