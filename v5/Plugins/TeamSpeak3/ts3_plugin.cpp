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

#include "ts3_plugin.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
Titus_Mutex hMutex;
TS3_CONFIG ts3_config;

static char ts3_desc[] = "TeamSpeak3";

struct TS3_REF_HANDLE {
	int server_index;
	int channel_id;
	int user_id;
};

#if !defined(DEBUG)
void ts3_up_ref(USER_PRESENCE * u) {
#else
void ts3_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("ts3_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	RelMutex(hMutex);
}

#if !defined(DEBUG)
void ts3_up_unref(USER_PRESENCE * u) {
#else
void ts3_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("ts3_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("ts3_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u->Ptr1);
		zfree(u);
	}
	RelMutex(hMutex);
}

bool send_ts3_pm(USER_PRESENCE * ut, const char * str) {
	LockMutex(hMutex);
	TS3_REF_HANDLE * h = (TS3_REF_HANDLE *)ut->Ptr1;
	if (ts3_config.servers[h->server_index].cli->IsConnected()) {
		if (h->channel_id) {
			ts3_config.servers[h->server_index].cli->SendChan(h->channel_id, str);
		} else {
			ts3_config.servers[h->server_index].cli->SendPM(h->user_id, str);
		}
	}
	RelMutex(hMutex);
	return true;
}

USER_PRESENCE * alloc_ts3_presence(USER * user, int server_index, uint32 user_id, const char * username, const char * hostmask, int channel_id = 0, const char * channel = NULL) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	TS3_REF_HANDLE * h = znew(TS3_REF_HANDLE);
	memset(h, 0, sizeof(TS3_REF_HANDLE));
	h->server_index = server_index;
	h->user_id = user_id;
	h->channel_id = channel_id;

	ret->User = user;
	ret->Nick = zstrdup(username);
	ret->Hostmask = zstrdup(hostmask);
	ret->NetNo = -1;
	ret->Flags = user ? user->Flags:api->user->GetDefaultFlags();
	if (channel && channel[0]) {
		ret->Channel = zstrdup(channel);
	}
	ret->Ptr1 = h;

	ret->send_chan_notice = send_ts3_pm;
	ret->send_chan = send_ts3_pm;
	ret->send_msg = send_ts3_pm;
	ret->send_notice = send_ts3_pm;
	ret->std_reply = send_ts3_pm;
	ret->Desc = ts3_desc;

	ret->ref = ts3_up_ref;
	ret->unref = ts3_up_unref;

	RefUser(ret);
	return ret;
};

void MyTS3_Client::UpdateTopic(STATS * s) {
	AutoMutex(hMutex);
	char buf[2048],buf2[32];
	string buf3;
	if (s->online) {
		sprintf(buf2, "TS3TopicOnline_%d", server_index);
		buf3 = "TS3TopicOnline";
	} else {
		sprintf(buf2, "TS3TopicOffline_%d", server_index);
		buf3 = "TS3TopicOffline";
	}
	if (api->LoadMessage(buf2, buf, sizeof(buf)) || api->LoadMessage(buf3.c_str(), buf, sizeof(buf))) {
		api->ProcText(buf, sizeof(buf));
		if (stricmp(MyChannelTopic().c_str(), buf)) {
			SetChannelTopic(buf);
		}
	}
	if (s->online) {
		sprintf(buf2, "TS3DescriptionOnline_%d", server_index);
		buf3 = "TS3DescriptionOnline";
	} else {
		sprintf(buf2, "TS3DescriptionOffline_%d", server_index);
		buf3 = "TS3DescriptionOffline";
	}
	if (api->LoadMessage(buf2, buf, sizeof(buf)) || api->LoadMessage(buf3.c_str(), buf, sizeof(buf))) {
		api->ProcText(buf, sizeof(buf));
		if (stricmp(MyChannelDescription().c_str(), buf)) {
			SetChannelDesc(buf);
		}
	}

	if (s->online && s->cursong[0]) {
		sprintf(buf2, "TS3Song_%d", server_index);
		buf3 = "TS3Song";
		if (api->LoadMessage(buf2, buf, sizeof(buf)) || api->LoadMessage(buf3.c_str(), buf, sizeof(buf))) {
			api->ProcText(buf, sizeof(buf));
			SendChan(MyChannelID(), buf);
		}
	}
}

void MyTS3_Client::OnPM(string snick, int nick_id, string smsg) {
	char * nick = zstrdup(snick.c_str());
	char * msg = zstrdup(smsg.c_str());
	str_replaceA(nick, strlen(nick)+1, " ", "_");
	char * hostmask = zmprintf("%s!%s@%d.teamspeak3.com", nick, nick, server_index);
	USER * user = api->user->GetUser(hostmask);
	uint32 flags = user ? user->Flags:api->user->GetDefaultFlags();
	char buf[64];
	api->user->uflag_to_string(flags, buf, sizeof(buf));
	api->ib_printf(_("TS3[%d] -> Message from %s[%s]: %s\n"), server_index, nick, buf, msg);

	StrTokenizer st(msg, ' ');
	char * cmd2 = st.GetSingleTok(1);
	char * cmd = cmd2;
	char * parms = NULL;
	if (st.NumTok() > 1) {
		parms = st.GetTok(2,st.NumTok());
	}

	if (api->commands->IsCommandPrefix(cmd[0])) {
		cmd++;
	}

	if (api->commands->IsCommandPrefix(cmd2[0]) || !ts3_config.RequirePrefix) {
		COMMAND * com = api->commands->FindCommand(cmd);
		if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
			USER_PRESENCE * up = alloc_ts3_presence(user, server_index, nick_id, nick, hostmask);
			RelMutex(hMutex);
			api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE);
			LockMutex(hMutex);
			UnrefUser(up);
		}
	}
	if (parms) { st.FreeString(parms); }
	st.FreeString(cmd2);
	if (user) { UnrefUser(user); }
	zfree(msg);
	zfree(hostmask);
	zfree(nick);
}

void MyTS3_Client::OnChan(string snick, int nick_id, int channel_id, string channel_name, string smsg) {
	char * nick = zstrdup(snick.c_str());
	char * msg = zstrdup(smsg.c_str());
	str_replaceA(nick, strlen(nick)+1, " ", "_");
	char * chan = zstrdup(channel_name.c_str());
	str_replaceA(chan, strlen(chan)+1, " ", "_");
	char * hostmask = zmprintf("%s!%s@%d.teamspeak3.com", nick, nick, server_index);
	USER * user = api->user->GetUser(hostmask);
	uint32 flags = user ? user->Flags:api->user->GetDefaultFlags();
	char buf[64];
	api->user->uflag_to_string(flags, buf, sizeof(buf));
	//api->ib_printf(_("TS3[%d] -> Channel message from %s[%s]: %s\n"), server_index, nick, buf, msg);

	StrTokenizer st(msg, ' ');
	char * cmd2 = st.GetSingleTok(1);
	char * cmd = cmd2;
	char * parms = NULL;
	if (st.NumTok() > 1) {
		parms = st.GetTok(2,st.NumTok());
	}

	if (api->commands->IsCommandPrefix(cmd[0])) {
		cmd++;
	}

	if (api->commands->IsCommandPrefix(cmd2[0]) || !ts3_config.RequirePrefix) {
		COMMAND * com = api->commands->FindCommand(cmd);
		if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PUBLIC)) {
			USER_PRESENCE * up = alloc_ts3_presence(user, server_index, nick_id, nick, hostmask, channel_id, chan);
			RelMutex(hMutex);
			api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE);
			LockMutex(hMutex);
			UnrefUser(up);
		}
	}
	if (parms) { st.FreeString(parms); }
	st.FreeString(cmd2);
	if (user) { UnrefUser(user); }
	zfree(msg);
	zfree(hostmask);
	zfree(nick);
	zfree(chan);
}

int voidptr2int(void * ptr) {
#if defined(__x86_64__)
	int64 i64tmp = (int64)ptr;
	return i64tmp;
#else
	return int(ptr);
#endif
}

THREADTYPE TS3Thread(void * lpData) {
	TT_THREAD_START
	int ind = voidptr2int(tt->parm);
	LockMutex(hMutex);
	MyTS3_Client * cli = ts3_config.servers[ind].cli = new MyTS3_Client();
	cli->server_index = ind;
	RelMutex(hMutex);
	//time_t lastRecv = 0;
	//time_t lastTry = 0;
	time_t nextTry = time(NULL);
	while (cli != NULL && !ts3_config.shutdown_now) {
		if (cli->IsConnected()) {
			cli->Work();
		} else if (!cli->IsConnected() && time(NULL) >= nextTry) {
			//lastTry = time(NULL);
			vector<string> tokens;
			/*
			for (int i=0; i < TS3_MAX_TOKENS; i++) {
				if (ts3_config.servers[ind].tokens[i][0]) {
					tokens.push_back(ts3_config.servers[ind].tokens[i]);
				}
			}
			*/
			if (!cli->Connect(ts3_config.servers[ind].host, ts3_config.servers[ind].username, ts3_config.servers[ind].password, ts3_config.servers[ind].port, ts3_config.servers[ind].server_id, ts3_config.servers[ind].channel, tokens.size() > 0 ? &tokens:NULL)) {
				api->ib_printf("TS3[%d] -> Error connecting to TS3 server: %s\n", ind, cli->GetError().c_str());
				nextTry = time(NULL)+30;
			}
			cli->SetClientNick(ts3_config.servers[ind].nick[0] ? ts3_config.servers[ind].nick:api->irc->GetDefaultNick(-1));
		}
		safe_sleep(1);
	}
	LockMutex(hMutex);
	if (cli) { delete cli; }
	ts3_config.servers[ind].cli = NULL;
	RelMutex(hMutex);
	TT_THREAD_END
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;

	api->ib_printf(_("TS3 -> TS3 support loading...\n"));

	if (api->irc == NULL || api->ss == NULL) {
		api->ib_printf2(pluginnum,_("TS3 -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	dsl_init();
	memset(&ts3_config, 0, sizeof(ts3_config));

	int have_serv = 0;

	DS_CONFIG_SECTION * root = api->config->GetConfigSection(NULL, "TeamSpeak3");
	if (root) {
		ts3_config.RequirePrefix = api->config->GetConfigSectionLong(root, "RequirePrefix") == 0 ? false:true;

		char buf[128];
		for (int i=0; i < TS3_MAX_SERVERS; i++) {
			sprintf(buf, "Server%d", i);
			DS_CONFIG_SECTION * sec = api->config->GetConfigSection(root, buf);
			if (sec) {
				sstrcpy(ts3_config.servers[i].username, api->irc->GetDefaultNick(-1));
				ts3_config.servers[i].port = TS3_PORT;
				ts3_config.servers[i].server_id = 0;
				char * p = api->config->GetConfigSectionValue(sec, "Host");
				if (p) {
					sstrcpy(ts3_config.servers[i].host, p);
				}
				p = api->config->GetConfigSectionValue(sec, "User");
				if (p) {
					sstrcpy(ts3_config.servers[i].username, p);
				}
				p = api->config->GetConfigSectionValue(sec, "Pass");
				if (p) {
					sstrcpy(ts3_config.servers[i].password, p);
				}
				p = api->config->GetConfigSectionValue(sec, "Nick");
				if (p) {
					sstrcpy(ts3_config.servers[i].nick, p);
				}
				p = api->config->GetConfigSectionValue(sec, "Channel");
				if (p) {
					sstrcpy(ts3_config.servers[i].channel, p);
				}
				int n = api->config->GetConfigSectionLong(sec, "Port");
				if (n > 0 && n <= 65535) {
					ts3_config.servers[i].port = n;
				}
				n = api->config->GetConfigSectionLong(sec, "ServerID");
				if (n > 0) {
					ts3_config.servers[i].server_id = n;
				}
				/*
				for (int j=0; j < TS3_MAX_TOKENS; j++) {
					sprintf(buf, "Token%d", j);
					p = api->config->GetConfigSectionValue(sec, buf);
					if (p) {
						sstrcpy(ts3_config.servers[i].tokens[j], p);
					}
				}
				sprintf(buf, "TS3Comment%d", i);
				if (!api->LoadMessage(buf, ts3_config.servers[i].comment, sizeof(ts3_config.servers[i].comment))) {
					if (!api->LoadMessage("TS3Comment", ts3_config.servers[i].comment, sizeof(ts3_config.servers[i].comment))) {
						sstrcpy(ts3_config.servers[i].comment, "I am a bot, message me !commands for a command list...");
					}
				}
				*/
				if (ts3_config.servers[i].host[0] && ts3_config.servers[i].username[0] && ts3_config.servers[i].password[0]) {
					api->ib_printf(_("TS3 -> Starting connection %d to %s:%d...\n"), i, ts3_config.servers[i].host, ts3_config.servers[i].port);
					TT_StartThread(TS3Thread, (void *)i, _("TS3 Thread"), pluginnum);
					have_serv = 1;
				}
			}
		}
		if (have_serv == 0) {
			api->ib_printf(_("TS3 -> No servers defined in ircbot.conf, disabling plugin...\n"));
		}
	} else {
		api->ib_printf(_("TS3 -> No 'TS3' section in ircbot.conf, disabling plugin...\n"));
	}

	return have_serv;
}

void quit() {
	ts3_config.shutdown_now = true;
	api->ib_printf(_("TS3 -> Shutting down...\n"));
	while (TT_NumThreadsWithID(pluginnum)) {
		safe_sleep(100,true);
	}
	api->ib_printf(_("TS3 -> Shut down.\n"));
	dsl_cleanup();
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
#if !defined(IRCBOT_LITE)
		case IB_SS_DRAGCOMPLETE:{
				if (*((bool *)data) == true) {
					STATS * stats = api->ss->GetStreamInfo();
					for (int i=0; i < TS3_MAX_SERVERS; i++) {
						if (ts3_config.servers[i].cli && ts3_config.servers[i].cli->IsConnected()) {
							ts3_config.servers[i].cli->UpdateTopic(stats);
						}
					}
				}
			}
			break;
#endif
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{1083C674-B89B-4F4E-B49D-218B17CD3657}",
	"TS3",
	"TS3 Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
