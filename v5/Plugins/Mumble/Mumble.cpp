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

#include "mumble_plugin.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
Titus_Mutex hMutex;
MUMBLE_CONFIG mumble_config;

static char mumble_desc[] = "Mumble";

struct MUMBLE_REF_HANDLE {
	int server_index;
	uint32 channel_id;
	uint32 user_id;
};

#if !defined(DEBUG)
void mumble_up_ref(USER_PRESENCE * u) {
#else
void mumble_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("mumble_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	RelMutex(hMutex);
}

#if !defined(DEBUG)
void mumble_up_unref(USER_PRESENCE * u) {
#else
void mumble_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("mumble_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("mumble_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u->Ptr1);
		zfree(u);
	}
	RelMutex(hMutex);
}

bool send_mumble_pm(USER_PRESENCE * ut, const char * str) {
	LockMutex(hMutex);
	//uint32 target_id = MUMBLE_INVALID_ID;
	//bool is_chan = false;
	MUMBLE_REF_HANDLE * h = (MUMBLE_REF_HANDLE *)ut->Ptr1;
	if (mumble_config.servers[h->server_index].cli->GetConnectionState() == MumbleEnums::Connected) {
		if (ut->Channel) {
			mumble_config.servers[h->server_index].cli->SendTextMessage(h->channel_id, str, true);
		} else {
			mumble_config.servers[h->server_index].cli->SendTextMessage(h->user_id, str, false);
		}
	}
	RelMutex(hMutex);
	return true;
}

USER_PRESENCE * alloc_mumble_presence(USER * user, int server_index, uint32 user_id, const char * username, const char * hostmask, const char * channel=NULL, uint32 channel_id=MUMBLE_INVALID_ID) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	MUMBLE_REF_HANDLE * h = znew(MUMBLE_REF_HANDLE);
	memset(h, 0, sizeof(MUMBLE_REF_HANDLE));
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

	ret->send_chan_notice = send_mumble_pm;
	ret->send_chan = send_mumble_pm;
	ret->send_msg = send_mumble_pm;
	ret->send_notice = send_mumble_pm;
	ret->std_reply = send_mumble_pm;
	ret->Desc = mumble_desc;

	ret->ref = mumble_up_ref;
	ret->unref = mumble_up_unref;

	RefUser(ret);
	return ret;
};

void MyMumbleClient::JoinMyChan() {
	AutoMutex(hMutex);
	if (mumble_config.servers[server_index].channel[0]) {
		MumbleChannel chan = GetChannelByName(mumble_config.servers[server_index].channel);
		if (chan.id != MUMBLE_INVALID_ID) {
			if (MyChannelID() != chan.id) {
				api->ib_printf("Mumble[%d] -> Attempting to join channel '%s'...\n", server_index, mumble_config.servers[server_index].channel);
				JoinChannel(chan.id);
			}
		} else {
			api->ib_printf("Mumble[%d] -> Could not find channel '%s' to join...\n", server_index, mumble_config.servers[server_index].channel);
		}
	}
}

void MyMumbleClient::UpdateComment() {
	AutoMutex(hMutex);
	char buf[2048];
	if (mumble_config.servers[server_index].comment[0]) {
		sstrcpy(buf, mumble_config.servers[server_index].comment);
		api->ProcText(buf, sizeof(buf));
		if (stricmp(last_comment.c_str(), buf)) {
			SetComment(buf);
			last_comment = buf;
		}
	}
}

void MyMumbleClient::OnServerSync(MumbleProto::ServerSync ss) {
	api->ib_printf("Mumble[%d] -> Connected to Murmur server (my session ID: %u, username: %s)\n", server_index, MySessionID(), MyUsername().c_str());
	/*
	char * msg = zmprintf("ShoutIRC Bot v%s", api->GetVersionString());
	SetComment(msg);
	zfree(msg);
	*/
	last_comment = "";
	UpdateComment();
	if (mumble_config.servers[server_index].image[0]) {
		SetImageFromFile(mumble_config.servers[server_index].image);
	}
	JoinMyChan();
}
void MyMumbleClient::OnReject(MumbleProto::Reject r) {
	api->ib_printf("Mumble[%d] -> Error connecting to Murmur server: %s\n", server_index, GetError().c_str());
}
void MyMumbleClient::OnPermissionDenied(MumbleProto::PermissionDenied pd) {
	api->ib_printf("Mumble[%d] -> Permission denied: %s\n", server_index, GetError().c_str());
}

void MyMumbleClient::OnPing(MumbleProto::Ping p) {
	//api->ib_printf("Mumble[%d] -> Ping? Pong!\n", server_index);
	JoinMyChan();
}


void MyMumbleClient::OnTextMessage(MumbleProto::TextMessage tm) {
	uint32 user_id = tm.actor();
	MumbleUser u = GetUserBySession(user_id);
	if (u.session_id != MUMBLE_INVALID_ID) {
		char * chan = NULL;
		uint32 channel_id = MUMBLE_INVALID_ID;
		if (tm.channel_id().size() > 0) {
			uint32 tmp = *tm.channel_id().begin();
			MumbleChannel pchan = GetChannelByID(tmp);
			if (pchan.id == tmp) {
				channel_id = tmp;
				chan = zstrdup(pchan.name.c_str());
				str_replaceA(chan, strlen(chan)+1, " ", "_");
			}
		}
		char * nick = zstrdup(u.username.c_str());
		char * msg = zstrdup(tm.message().c_str());
		str_replaceA(nick, strlen(nick)+1, " ", "_");
		char * hostmask = zmprintf("%s!%s@%d.mumble.com", nick, nick, server_index);
		USER * user = api->user->GetUser(hostmask);
		uint32 flags = user ? user->Flags:api->user->GetDefaultFlags();
		char buf[64];
		api->user->uflag_to_string(flags, buf, sizeof(buf));
		api->ib_printf(_("Mumble[%d] -> Message from %s[%s]: %s\n"), server_index, nick, buf, msg);

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

		if (api->commands->IsCommandPrefix(cmd2[0]) || !mumble_config.RequirePrefix) {
			COMMAND * com = api->commands->FindCommand(cmd);
			uint32 type = (channel_id == MUMBLE_INVALID_ID) ? CM_ALLOW_CONSOLE_PRIVATE:CM_ALLOW_CONSOLE_PUBLIC;
			if (com && com->proc_type == COMMAND_PROC && (com->mask & type)) {
				USER_PRESENCE * up = alloc_mumble_presence(user, server_index, user_id, nick, hostmask, chan, channel_id);
				RelMutex(hMutex);
				api->commands->ExecuteProc(com, parms, up, type);
				LockMutex(hMutex);
				UnrefUser(up);
			}
		}
		if (parms) { st.FreeString(parms); }
		st.FreeString(cmd2);

		if (user) { UnrefUser(user); }
		if (chan) { zfree(chan); }
		zfree(msg);
		zfree(hostmask);
		zfree(nick);
	} else {
		api->ib_printf("Mumble[%d] -> Incoming text message from unknown user with ID '%u': %s\n", server_index, user_id, tm.message().c_str());
	}
}

int voidptr2int(void * ptr) {
#if defined(__x86_64__)
	int64 i64tmp = (int64)ptr;
	return i64tmp;
#else
	return int(ptr);
#endif
}

THREADTYPE MumbleThread(void * lpData) {
	TT_THREAD_START
	int ind = voidptr2int(tt->parm);
	LockMutex(hMutex);
	MyMumbleClient * cli = mumble_config.servers[ind].cli = new MyMumbleClient();
	cli->server_index = ind;
	//cli->SetSocks(api->socks);
	if (!cli->EnableSSL(mumble_config.servers[ind].cert_fn[0] ? mumble_config.servers[ind].cert_fn:"ircbot.pem")) {
		api->ib_printf(_("Mumble[%d] -> Error enabling SSL: %s!\n"), ind, cli->GetError().c_str());
		delete cli;
		cli = mumble_config.servers[ind].cli = NULL;
	}
	RelMutex(hMutex);
	//time_t lastRecv = 0;
	time_t lastTry = 0;
	time_t nextTry = time(NULL);
	while (cli != NULL && !mumble_config.shutdown_now) {
		if (cli->GetConnectionState() == MumbleEnums::Connected && (time(NULL) - cli->LastPingReply()) >= 60) {
			api->ib_printf(_("Mumble[%d] -> Timeout in server connection, attempting reconnect...\n"), ind);
			cli->Disconnect();
		} else if (cli->GetConnectionState() == MumbleEnums::Connecting && ((time(NULL) - lastTry) > 30)) {
			api->ib_printf(_("Mumble[%d] -> Timeout connecting to server!\n"), ind);
			cli->Disconnect();
			nextTry = time(NULL)+30;
		} else if (cli->GetConnectionState() > MumbleEnums::Disconnected) {
			cli->Work();
		} else if (cli->GetConnectionState() == MumbleEnums::Disconnected && time(NULL) >= nextTry) {
			lastTry = time(NULL);
			vector<string> tokens;
			for (int i=0; i < MUMBLE_MAX_TOKENS; i++) {
				if (mumble_config.servers[ind].tokens[i][0]) {
					tokens.push_back(mumble_config.servers[ind].tokens[i]);
				}
			}
			if (!cli->Connect(mumble_config.servers[ind].host, mumble_config.servers[ind].username, mumble_config.servers[ind].password[0] ? mumble_config.servers[ind].password:NULL, mumble_config.servers[ind].port, tokens.size() > 0 ? &tokens:NULL)) {
				api->ib_printf("Mumble[%d] -> Error connecting to Murmur server: %s\n", ind, cli->GetError().c_str());
			}
		}
		safe_sleep(1);
	}
	LockMutex(hMutex);
	if (cli) { delete cli; }
	mumble_config.servers[ind].cli = NULL;
	RelMutex(hMutex);
	TT_THREAD_END
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;
	dsl_init();

	api->ib_printf(_("Mumble -> Mumble support loading...\n"));
	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Mumble -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	memset(&mumble_config, 0, sizeof(mumble_config));

	int have_serv = 0;

	DS_CONFIG_SECTION * root = api->config->GetConfigSection(NULL, "Mumble");
	if (root) {
		mumble_config.RequirePrefix = api->config->GetConfigSectionLong(root, "RequirePrefix") == 0 ? false:true;

		char buf[128];
		for (int i=0; i < MUMBLE_MAX_SERVERS; i++) {
			sprintf(buf, "Server%d", i);
			DS_CONFIG_SECTION * sec = api->config->GetConfigSection(root, buf);
			if (sec) {
				sstrcpy(mumble_config.servers[i].cert_fn, "ircbot.pem");
				sstrcpy(mumble_config.servers[i].username, api->irc->GetDefaultNick(-1));
				mumble_config.servers[i].port = MUMBLE_PORT;
				char * p = api->config->GetConfigSectionValue(sec, "Host");
				if (p) {
					sstrcpy(mumble_config.servers[i].host, p);
				}
				p = api->config->GetConfigSectionValue(sec, "Nick");
				if (p) {
					sstrcpy(mumble_config.servers[i].username, p);
				}
				p = api->config->GetConfigSectionValue(sec, "SSL_Cert");
				if (p) {
					sstrcpy(mumble_config.servers[i].cert_fn, p);
				}
				p = api->config->GetConfigSectionValue(sec, "Pass");
				if (p) {
					sstrcpy(mumble_config.servers[i].password, p);
				}
				p = api->config->GetConfigSectionValue(sec, "Channel");
				if (p) {
					sstrcpy(mumble_config.servers[i].channel, p);
				}
				p = api->config->GetConfigSectionValue(sec, "Image");
				if (p) {
					sstrcpy(mumble_config.servers[i].image, p);
				}
				int n = api->config->GetConfigSectionLong(sec, "Port");
				if (n > 0 && n <= 65535) {
					mumble_config.servers[i].port = n;
				}
				for (int j=0; j < MUMBLE_MAX_TOKENS; j++) {
					sprintf(buf, "Token%d", j);
					p = api->config->GetConfigSectionValue(sec, buf);
					if (p) {
						sstrcpy(mumble_config.servers[i].tokens[j], p);
					}
				}
				sprintf(buf, "MumbleComment%d", i);
				if (!api->LoadMessage(buf, mumble_config.servers[i].comment, sizeof(mumble_config.servers[i].comment))) {
					if (!api->LoadMessage("MumbleComment", mumble_config.servers[i].comment, sizeof(mumble_config.servers[i].comment))) {
						sstrcpy(mumble_config.servers[i].comment, "I am a bot, message me !commands for a command list...");
					}
				}
				if (mumble_config.servers[i].host[0]) {
					api->ib_printf(_("Mumble -> Starting connection %d to %s:%d...\n"), i, mumble_config.servers[i].host, mumble_config.servers[i].port);
					TT_StartThread(MumbleThread, (void *)i, _("Mumble Thread"), pluginnum);
					have_serv = 1;
				}
			}
		}
		if (have_serv == 0) {
			api->ib_printf(_("Mumble -> No servers defined in ircbot.conf, disabling plugin...\n"));
		}
	} else {
		api->ib_printf(_("Mumble -> No 'Mumble' section in ircbot.conf, disabling plugin...\n"));
	}

	return have_serv;
}

void quit() {
	mumble_config.shutdown_now = true;
	api->ib_printf(_("Mumble -> Shutting down...\n"));
	while (TT_NumThreadsWithID(pluginnum)) {
		safe_sleep(100,true);
	}
	api->ib_printf(_("Mumble -> Shut down.\n"));
	dsl_cleanup();
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
#if !defined(IRCBOT_LITE)
		case IB_SS_DRAGCOMPLETE:{
				if (*((bool *)data) == true) {
					for (int i=0; i < MUMBLE_MAX_SERVERS; i++) {
						if (mumble_config.servers[i].cli && mumble_config.servers[i].cli->GetConnectionState() == MumbleEnums::Connected) {
							mumble_config.servers[i].cli->UpdateComment();
						}
					}
				}
			}
			break;
#endif

		case IB_BROADCAST_MSG:{
				if (api->irc->GetDoSpam()) {
					IBM_USERTEXT * ibm = (IBM_USERTEXT *)data;
					for (int i=0; i < MUMBLE_MAX_SERVERS; i++) {
						if (mumble_config.servers[i].cli && mumble_config.servers[i].cli->GetConnectionState() == MumbleEnums::Connected && mumble_config.servers[i].cli->MyChannelID() != MUMBLE_INVALID_ID) {
							mumble_config.servers[i].cli->SendTextMessage(mumble_config.servers[i].cli->MyChannelID(), ibm->text, true);
						}
					}
				}
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{1083C674-B89B-4F4E-B49D-218B17CD3657}",
	"Mumble",
	"Mumble Plugin 1.0",
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
