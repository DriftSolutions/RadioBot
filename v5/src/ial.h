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

#ifndef __INCLUDE_IAL_H__
#define __INCLUDE_IAL_H__
#if defined(IRCBOT_ENABLE_IRC)

#include <list>

#define IAL_CHUFLAG_OWNER	0x0001
#define IAL_CHUFLAG_ADMIN	0x0002
#define IAL_CHUFLAG_OP		0x0004
#define IAL_CHUFLAG_HALFOP	0x0008
#define IAL_CHUFLAG_VOICE	0x0010

class IAL;
class IAL_NICK {
public:
	IAL_NICK() { ref=0; }
	uint8 ref;///the number of reference to this nick (ie. the number of channels he/she is on). Nobody should be on >255 channels
	string nick;
	string hostmask;
};
typedef std::list<IAL_NICK> ialNickList;

class IAL_CHANNEL_NICKS {
public:
	IAL_NICK * Nick;
	uint8 chumode; // IAL_CHUFLAG_* above
	IAL_CHANNEL_NICKS() { chumode = 0; }
};
typedef std::list<IAL_CHANNEL_NICKS> ialChannelNickList;

class IAL_CHANNEL {
public:
	//string channel;
	ialChannelNickList nicks;

	//IAL_CHANNEL() { nickCount = 0; }
};
typedef std::map<string, IAL_CHANNEL, ci_less> ialChannelList;

class IAL {
private:
	ialChannelList channels;
	ialNickList nicks;

	IAL_NICK * GetNick(const char * nick, bool no_create = false);
	void DeleteNick(IAL_NICK * Nick);

	bool IsChan(const char * name);
	void DeleteChan(IAL_CHANNEL * chan);
	void DeleteChan(ialChannelList::iterator y);
	void UpdateHostmask(IAL_NICK * Nick, const char * hostmask);//Update a nick's hostmask

public:
	Titus_Mutex hMutex;

	IAL();
	~IAL();
	void Clear();
	void Dump();

	bool IsNickOnChan(const char * chan, const char * nick);
	IAL_NICK * GetNickByHostmask(const char * hostmask);

	IAL_CHANNEL * GetChan(const char * name, bool no_create = false);///Get a channel handle, no_create controls whether a channel handle is created if one is not found
	void DeleteChan(const char * name);///Deletes a channel from the IAL by its name.

	IAL_NICK * GetNick(IAL_CHANNEL * chan, const char * nick, const char * hostmask = NULL, bool no_create = false);///Get a nick's record if he is on the channel chan. If no_create == false then the user will be added to the channel and his/her ref increased
	IAL_CHANNEL_NICKS * GetNickChanEntry(IAL_CHANNEL * chan, const char * nick, bool no_create = false);///Get a nick's record if he is on the channel chan. If no_create == false then the user will be added to the channel and his/her ref increased
	void ChangeNick(const char * sold, const char * snew);///Change a nick's nick
	void DeleteNickFromAll(const char * nick);//Delete a nick from all channels that he is (ie. QUIT)
	void DeleteNick(IAL_CHANNEL * Chan, IAL_NICK * Nick);//Delete a nick from a channel (ie. PART)
};

/*
bool IsChan(char * name, int netno);
IAL_CHANNEL * GetChan(char * name, int netno, bool no_create = false);
void DeleteChan(char * name, int netno);

bool IsNickOnChan(char * chan, int netno, char * nick);

IAL_NICK * GetNick(IAL_CHANNEL * chan, char * nick, bool no_create = false);
void ChangeNick(char * sold, char * snew, int netno);
void DeleteNick(IAL_CHANNEL * Chan, IAL_NICK * Nick);
void DeleteNickFromAll(char * nick);

void ClearNet(int netno);
*/

#endif // defined(IRCBOT_ENABLE_IRC)
#endif // __INCLUDE_IAL_H__
