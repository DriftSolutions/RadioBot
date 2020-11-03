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

#include "purple_plugin.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
Titus_Mutex hMutex;
Titus_Sockets socks;
PIDGIN_CONFIG pidgin_config;

static char pidgin_desc[] = "Pidgin";

struct PIDGIN_REF_HANDLE {
	uint64 convid;
	bool is_chat;
	T_SOCKET * sock;
};

#if !defined(DEBUG)
void pidgin_up_ref(USER_PRESENCE * u) {
#else
void pidgin_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("pidgin_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	RelMutex(hMutex);
}

#if !defined(DEBUG)
void pidgin_up_unref(USER_PRESENCE * u) {
#else
void pidgin_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("pidgin_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("pidgin_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u->Ptr1);
		zfree(u);
	}
	RelMutex(hMutex);
}


int send_packet(T_SOCKET * s, unsigned char cmd, unsigned int len, char * data=NULL) {
	if (len <= SHOUTIRC_MAX_PACKET) {
		int i=0,left=sizeof(BRIDGE_PACKET)+len;
		char * buf = (char *)zmalloc(left);
		BRIDGE_PACKET * pack = (BRIDGE_PACKET *)buf;
		if (len > 0) {
			char * ptr = buf + sizeof(BRIDGE_PACKET);
			memcpy(ptr, data, len);
		}
		pack->cmd = cmd;
		pack->len = Get_UBE32(len);
		while (left > 0) {
			int n = socks.Send(s, &buf[i], left);
			if (n <= 0) {
				zfree(buf);
				return n;
			}
			left -= n;
			i += n;
		}
		zfree(buf);
		return i;
	} else {
		return -1;
	}
}

bool send_pidgin_pm(USER_PRESENCE * ut, const char * str) {
	LockMutex(hMutex);
	PIDGIN_REF_HANDLE * h = (PIDGIN_REF_HANDLE *)ut->Ptr1;
	if (pidgin_config.connected && socks.IsKnownSocket(h->sock)) {
		Titus_Buffer buf;
		buf.Append_uint64(h->convid);
		buf.Append(str, strlen(str)+1);
		if (h->is_chat) {
			send_packet(h->sock, IMB_CHAT_SEND, buf.GetLen(), buf.Get());
		} else {
			send_packet(h->sock, IMB_IM_SEND, buf.GetLen(), buf.Get());
		}
	}
	RelMutex(hMutex);
	return true;
}

USER_PRESENCE * alloc_pidgin_presence(USER * user, uint64 convid, const char * username, const char * hostmask, const char * channel = NULL) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	PIDGIN_REF_HANDLE * h = znew(PIDGIN_REF_HANDLE);
	memset(h, 0, sizeof(PIDGIN_REF_HANDLE));
	h->convid = convid;
	h->is_chat = (channel && *channel) ? true:false;
	h->sock = pidgin_config.sock;

	ret->User = user;
	ret->Nick = zstrdup(username);
	ret->Hostmask = zstrdup(hostmask);
	ret->NetNo = -1;
	ret->Flags = user ? user->Flags:api->user->GetDefaultFlags();
	if (channel && *channel) {
		ret->Channel = zstrdup(channel);
	}
	ret->Ptr1 = h;

	ret->send_chan_notice = send_pidgin_pm;
	ret->send_chan = send_pidgin_pm;
	ret->send_msg = send_pidgin_pm;
	ret->send_notice = send_pidgin_pm;
	ret->std_reply = send_pidgin_pm;
	ret->Desc = pidgin_desc;

	ret->ref = pidgin_up_ref;
	ret->unref = pidgin_up_unref;

	RefUser(ret);
	return ret;
};

bool recv_packet(T_SOCKET * s, BRIDGE_PACKET * pack, char * data, size_t dataSize) {
	if (pack == NULL || (dataSize && data == NULL)) {
		return false;
	}
	int n = socks.Recv(s, (char *)pack, sizeof(BRIDGE_PACKET));
	if (n < sizeof(BRIDGE_PACKET)) {
		return false;
	}
	pack->len = Get_UBE32(pack->len);
	if (pack->len > SHOUTIRC_MAX_PACKET || pack->len > dataSize) {
		return false;
	}
	if (data && pack->len > 0) {
		unsigned int left = pack->len;
		unsigned int i = 0;
		while (left > 0) {
			int n = socks.Recv(s, data+i, left);
			if (n <= 0) {
				return false;
			}
			left -= n;
			i += n;
		}
	}

	if (data) { data[dataSize-1] = 0; }
	return true;
}

void on_im(uint64 convid, char * sproto, char * snick, char * msg) {
	char * nick = zstrdup(snick);
	str_replaceA(nick, strlen(nick)+1, " ", "_");
	str_replaceA(nick, strlen(nick)+1, "!", "_");
	str_replaceA(nick, strlen(nick)+1, "@", "_");
	char * proto = zstrdup(sproto);
	str_replaceA(proto, strlen(proto)+1, " ", ".");
	str_replaceA(proto, strlen(proto)+1, "!", ".");
	str_replaceA(proto, strlen(proto)+1, "@", ".");
	for (size_t i=0; i < strlen(proto); i++) {
		proto[i] = tolower(proto[i]);
	}
	char * hostmask = zmprintf("%s!%s@im.pidgin.com", nick, proto);
	USER * user = api->user->GetUser(hostmask);
	uint32 flags = user ? user->Flags:api->user->GetDefaultFlags();
	char buf[64];
	api->user->uflag_to_string(flags, buf, sizeof(buf));
	api->ib_printf(_("Pidgin[%s] -> IM from %s[%s]: %s\n"), proto, nick, buf, msg);

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

	if (api->commands->IsCommandPrefix(cmd2[0]) || !pidgin_config.RequirePrefix) {
		COMMAND * com = api->commands->FindCommand(cmd);
		if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
			USER_PRESENCE * up = alloc_pidgin_presence(user, convid, nick, hostmask);
			api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE);
			UnrefUser(up);
		}
	}
	if (parms) { st.FreeString(parms); }
	st.FreeString(cmd2);
	if (user) { UnrefUser(user); }
	zfree(proto);
	zfree(hostmask);
	zfree(nick);
}

void on_chat(uint64 convid, char * sproto, char * snick, char * schannel, char * msg) {
	char * nick = zstrdup(snick);
	str_replaceA(nick, strlen(nick)+1, " ", "_");
	str_replaceA(nick, strlen(nick)+1, "!", "_");
	str_replaceA(nick, strlen(nick)+1, "@", "_");
	char * proto = zstrdup(sproto);
	str_replaceA(proto, strlen(proto)+1, " ", ".");
	str_replaceA(proto, strlen(proto)+1, "!", ".");
	str_replaceA(proto, strlen(proto)+1, "@", ".");
	char * channel = zstrdup(schannel);
	str_replaceA(channel, strlen(channel)+1, " ", ".");
	str_replaceA(channel, strlen(channel)+1, "!", ".");
	str_replaceA(channel, strlen(channel)+1, "@", ".");
	char * hostmask = zmprintf("%s!%s@%d.pidgin.com", nick, nick, proto);
	USER * user = api->user->GetUser(hostmask);
	uint32 flags = user ? user->Flags:api->user->GetDefaultFlags();
	char buf[64];
	api->user->uflag_to_string(flags, buf, sizeof(buf));
	api->ib_printf(_("Pidgin[%s] -> Channel/group message from %s[%s] in %s: %s\n"), proto, nick, buf, channel, msg);

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

	if (api->commands->IsCommandPrefix(cmd2[0]) || !pidgin_config.RequirePrefix) {
		COMMAND * com = api->commands->FindCommand(cmd);
		if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PUBLIC)) {
			USER_PRESENCE * up = alloc_pidgin_presence(user, convid, nick, hostmask, channel);
			api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PUBLIC);
			UnrefUser(up);
		}
	}
	if (parms) { st.FreeString(parms); }
	st.FreeString(cmd2);
	if (user) { UnrefUser(user); }
	zfree(hostmask);
	zfree(nick);
	zfree(proto);
	zfree(channel);
}

THREADTYPE PidginThread(void * lpData) {
	TT_THREAD_START
	LockMutex(hMutex);
	pidgin_config.sock = NULL;
	pidgin_config.connected = false;
	RelMutex(hMutex);
	BRIDGE_PACKET pack;
	char data[SHOUTIRC_MAX_PACKET];
	//time_t lastTry = 0;
	time_t nextTry = time(NULL);
	while (!pidgin_config.shutdown_now) {
		if (pidgin_config.sock) {
			if (socks.Select_Read(pidgin_config.sock, 1) == 1) {
				if (recv_packet(pidgin_config.sock, &pack, data, sizeof(data))) {
					if (pack.cmd == IMB_IM_RECV) {
						char * ptr = data;
						uint64 conv = *((uint64 *)ptr);
						ptr += sizeof(uint64);
						char * proto = ptr;
						ptr += strlen(proto)+1;
						char * nick = ptr;
						ptr += strlen(nick)+1;
						char * text = ptr;
						//printf("[%s] IM from %s ("U64FMT"): %s\n", proto, nick, conv, text);
						on_im(conv, proto, nick, text);
					} else if (pack.cmd == IMB_CHAT_RECV) {
						char * ptr = data;
						uint64 conv = *((uint64 *)ptr);
						ptr += sizeof(uint64);
						char * proto = ptr;
						ptr += strlen(proto)+1;
						char * nick = ptr;
						ptr += strlen(nick)+1;
						char * chan = ptr;
						ptr += strlen(chan)+1;
						char * text = ptr;
						//printf("[%s] Chat from %s in %s ("U64FMT"): %s\n", proto, nick, chan, conv, text);
						on_chat(conv, proto, nick, chan, text);
					}
				} else {
					LockMutex(hMutex);
					pidgin_config.connected = false;
					socks.Close(pidgin_config.sock);
					pidgin_config.sock = NULL;
					RelMutex(hMutex);
				}
			}
			//cli->Work();
		} else if (pidgin_config.sock == NULL && time(NULL) >= nextTry) {
			//lastTry = time(NULL);
			pidgin_config.sock = socks.Create();
			if (socks.ConnectWithTimeout(pidgin_config.sock, pidgin_config.host, pidgin_config.port, 10000)) {
				socks.SetKeepAlive(pidgin_config.sock);
				socks.SetRecvTimeout(pidgin_config.sock, 30000);
				socks.SetSendTimeout(pidgin_config.sock, 30000);

				if (socks.Select_Read(pidgin_config.sock, 10000) == 1 && recv_packet(pidgin_config.sock, &pack, data, sizeof(data))) {
					if (pack.cmd == IMB_WELCOME) {
						send_packet(pidgin_config.sock, IMB_AUTH, strlen(pidgin_config.password)+1, pidgin_config.password);
						if (socks.Select_Read(pidgin_config.sock, 10000) == 1 && recv_packet(pidgin_config.sock, &pack, data, sizeof(data))) {
							if (pack.cmd == IMB_AUTH_OK) {
								api->ib_printf(_("Pidgin -> Connected to bridge...\n"));
								LockMutex(hMutex);
								pidgin_config.connected = true;
								RelMutex(hMutex);
							} else {
								api->ib_printf(_("Pidgin -> Error connecting to bridge: password not accepted...\n"));
							}
						} else {
							api->ib_printf(_("Pidgin -> Did not receive an auth reply within 10 seconds...\n"));
						}
					} else if (pack.cmd == IMB_FULL) {
						api->ib_printf(_("Pidgin -> Bridge is full, trying again later...\n"));
					} else {
						api->ib_printf(_("Pidgin -> Unknown packet received during handshake!\n"));
					}
				} else {
					api->ib_printf(_("Pidgin -> Did not receive a welcome packet within 10 seconds...\n"));
				}
			} else {
				api->ib_printf(_("Pidgin -> Error connecting to IM bridge: %s\n"), socks.GetLastErrorString());
			}
			LockMutex(hMutex);
			if (!pidgin_config.connected) {
				nextTry = time(NULL)+30;
				socks.Close(pidgin_config.sock);
				pidgin_config.sock = NULL;
			}
			RelMutex(hMutex);
		}
		safe_sleep(1);
	}
	LockMutex(hMutex);
	if (pidgin_config.connected) {
		send_packet(pidgin_config.sock, IMB_QUIT, 0);
	}
	if (pidgin_config.sock) {
		socks.Close(pidgin_config.sock);
	}
	pidgin_config.connected = false;
	pidgin_config.sock = NULL;
	RelMutex(hMutex);
	TT_THREAD_END
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;

	dsl_init();
	memset(&pidgin_config, 0, sizeof(pidgin_config));

	pidgin_config.port = SHOUTIRC_BRIDGE_PORT;
	strcpy(pidgin_config.host, "127.0.0.1");

	DS_CONFIG_SECTION * root = api->config->GetConfigSection(NULL, "Pidgin");
	if (root) {
		pidgin_config.RequirePrefix = api->config->GetConfigSectionLong(root, "RequirePrefix") == 0 ? false:true;

		char * p = api->config->GetConfigSectionValue(root, "Host");
		if (p) {
			sstrcpy(pidgin_config.host, p);
		}
		p = api->config->GetConfigSectionValue(root, "Pass");
		if (p) {
			sstrcpy(pidgin_config.password, p);
		}
		int n = api->config->GetConfigSectionLong(root, "Port");
		if (n > 0 && n <= 65535) {
			pidgin_config.port = n;
		}
	} else {
		api->ib_printf(_("Pidgin -> No 'Pidgin' section in ircbot.conf, using defaults...\n"));
	}

	TT_StartThread(PidginThread, NULL, _("Pidgin Thread"), pluginnum);

	return 1;
}

void quit() {
	pidgin_config.shutdown_now = true;
	api->ib_printf(_("Pidgin -> Shutting down...\n"));
	while (TT_NumThreadsWithID(pluginnum)) {
		safe_sleep(100,true);
	}
	api->ib_printf(_("Pidgin -> Shut down.\n"));
	dsl_cleanup();
}

int message(unsigned int msg, char * data, int datalen) {
	/*
switch (msg) {
#if !defined(IRCBOT_LITE)
		case IB_SS_DRAGCOMPLETE:{
				if (*((bool *)data) == true) {
					STATS * stats = api->ss->GetStreamInfo();
					for (int i=0; i < PIDGIN_MAX_SERVERS; i++) {
						if (pidgin_config.cli && pidgin_config.cli->IsConnected()) {
							pidgin_config.cli->UpdateTopic(stats);
						}
					}
				}
			}
			break;
#endif
	}
	*/
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{065EE30D-F97F-4DAA-BE85-EA65E783B5B1}",
	"Pidgin",
	"Pidgin/libpurple IM Plugin 1.0",
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
