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

#ifndef MEMLEAK
#define MEMLEAK
#endif
#include "../../src/plugins.h"

struct SMS_CONFIG {
	bool enabled, shutdown_now;
	bool RequirePrefix, LineMerge;
	int HostmaskForm;
	struct {
		char user[128];
		char pass[128];
		char host[128];
		int port;
		int ssl;
	} pop3;
	struct {
		char user[128];
		char pass[128];
	} voice;
};

#include "sms_interface.h"

extern SMS_CONFIG sms_config;
extern int pluginnum;
extern BOTAPI_DEF * api;

TT_DEFINE_THREAD(POP_Thread);
TT_DEFINE_THREAD(Send_Thread);

class SMS_Message {
public:
	string from;
	string to;
	string message;
};
typedef std::vector<SMS_Message> msgList;
typedef std::map<string, string> msgMap;


struct MEMWRITE {
	char * data;
	size_t len;
};

class GoogleVoice {
private:
	string user, pass;
	string google_priv;
	CURL * ch;
	MEMWRITE mem;

	bool logged_in;

public:
	GoogleVoice(string puser, string ppass);
	~GoogleVoice();

	bool SendSMS(string phone, string msg);

	bool logIn();
};

void Send_SMS(string phone, string msg);
