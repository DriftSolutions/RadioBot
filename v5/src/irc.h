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


#ifndef __INCLUDE_IRCBOT_IRC_H__
#define __INCLUDE_IRCBOT_IRC_H__
#if defined(IRCBOT_ENABLE_IRC)

THREADTYPE ircThread(void *lpData);
char * strip_control_codes(unsigned char *buf);

#define NUM_IRC_MODES 52
#define MAX_IRC_CHUMODES 8
#define DEFAULT_CHAN_MODES "b,k,l,mnpst"

class CHANNELS {
public:
	bool Reset(bool first);

	std::string channel;
	std::string key;
	std::string topic;
	std::string altTopicCommand;
	std::string altJoinCommand;
	char modes[32];

	bool autoVoice;
	bool moderateOnUpdate;
	bool dospam;
	bool dotopic;
	bool doonjoin;
	bool joined;
	bool noTopicCheck;

	int32 songinterval,songintervalsource;
	int64 lastsong;
};

CHANNELS * GetChannel(int netno, const char * chan);
void JoinChannel(int netno, const char * chan, const char * key);
void JoinChannel(int netno, CHANNELS * chan);

class IRCNETS {
public:
	void Reset();

	std::string cur_nick;
	std::string base_nick;
	std::string host;
	std::string pass;
	std::string bindip;
	std::string server;
	std::string network;
	std::string onconnect;
	std::string onnickinusecommand;
	std::string on512;
	std::string removePreText;
	std::string custIdent;

	char my_mode[32];

	int32 port;
	int32 num_chans;
	int32 ssl;

	CHANNELS channels[16];
	T_SOCKET * sock;
	FILE * log_fp;
	time_t last_recv, readyTime;
	IAL * ial;

	uint8 chmode_handling[NUM_IRC_MODES];
	uint8 ch_umodes[MAX_IRC_CHUMODES*2];
	char channel_types[8];
	bool IsChannelPrefix(char c);
	void ResetChanModes();
	void UpdateChanModesFromUModes();
	void LoadChanModes(string str=DEFAULT_CHAN_MODES, bool reset=false);
	void LoadChUserModes(string str="(ohv)@%+");

	bool shortIdent;
	bool log;
	bool regainNick;
	bool enable_cap;
	//bool has_owner;
	//bool has_namesx;
	bool ready;
};

#endif // defined(IRCBOT_ENABLE_IRC)
#endif
