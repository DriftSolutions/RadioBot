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

#include "jabber.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
Titus_Mutex hMutex;
Titus_Sockets socks;
JABBER_CONFIG j_config;

static char jabber_desc[] = "Jabber";

#if !defined(DEBUG)
void jabber_up_ref(USER_PRESENCE * u) {
#else
void jabber_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("jabber_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	hMutex.Lock();
	if (u->User) {
		RefUser(u->User);
	}
	u->ref_cnt++;
	hMutex.Release();
}

#if !defined(DEBUG)
void jabber_up_unref(USER_PRESENCE * u) {
#else
void jabber_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("jabber_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	hMutex.Lock();
	if (u->User) {
		UnrefUser(u->User);
	}
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("jabber_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u);
	}
	hMutex.Release();
}

bool send_jabber_pm(USER_PRESENCE * ut, const char * str) {
	bool ret = true;
	hMutex.Lock();
	GError * error = NULL;
	const char * dest = ut->Hostmask;
	const char * p = strchr(dest, '!');
	if (p) { dest = p+1; }
	LmMessage * m = lm_message_new(dest, LM_MESSAGE_TYPE_MESSAGE);
	lm_message_node_add_child(m->node, "body", str);
#if defined(DEBUG)
	api->ib_printf("Jabber -> Sending message to %s: %s\n", dest, str);
#endif
	if (!lm_connection_send(j_config.connection, m, &error)) {
		api->ib_printf("Jabber -> Error sending message to %s (%s): %s\n", ut->Nick, dest, error->message);
		ret = false;
	}
	lm_message_unref(m);
	hMutex.Release();
	return ret;
}

USER_PRESENCE * alloc_jabber_presence(USER * user, const char * username, const char * hostmask) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = user;
	ret->Nick = zstrdup(username);
	ret->Hostmask = zstrdup(hostmask);
	ret->NetNo = -1;
	ret->Flags = user ? user->Flags:api->user->GetDefaultFlags();

	ret->send_chan_notice = send_jabber_pm;
	ret->send_chan = send_jabber_pm;
	ret->send_msg = send_jabber_pm;
	ret->send_notice = send_jabber_pm;
	ret->std_reply = send_jabber_pm;
	ret->Desc = jabber_desc;

	ret->ref = jabber_up_ref;
	ret->unref = jabber_up_unref;

	RefUser(ret);
	return ret;
};

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;
	return true;
}

void quit() {
}

LmHandlerResult Jabber_CB(LmMessageHandler *handler, LmConnection *connection, LmMessage * m, gpointer user_data) {
	api->ib_printf("JibJab: %s\n", lm_message_node_to_string(lm_message_get_node(m)));

	const char * hostmask = lm_message_node_get_attribute(m->node, "from");
	const char * type = lm_message_node_get_attribute(m->node, "type");
	if (hostmask && type) {
		char * from = (char *)zstrdup(hostmask);
		char * p = strchr(from, '@');
		if (p) { p[0]=0; }
		LmMessageNode * n = lm_message_node_get_child(m->node, "body");
		const char * txt = NULL;
		if (n && (txt = lm_message_node_get_value(n)) != NULL && strlen(txt)) {
			char hostmask2[1024];
			sprintf(hostmask2, "%s!%s", from, hostmask);

			USER * user = api->user->GetUser(hostmask2);
			uint32 uflags = user ? user->Flags:api->user->GetDefaultFlags();
			api->ib_printf("Jabber -> Message from %s[%d]: %s\n", from, uflags, txt);

			StrTokenizer st((char *)txt, ' ');
			char * cmd2 = st.GetSingleTok(1);
			char * cmd = cmd2;
			char * parms = NULL;
			if (st.NumTok() > 1) {
				parms = st.GetTok(2,st.NumTok());
			}
			if (api->commands->IsCommandPrefix(cmd[0])) {
				cmd++;
			}

			//printf("cmd: %s, %d\n", cmd, api->commands->IsCommandPrefix(cmd2[0]));
			if (api->commands->IsCommandPrefix(cmd2[0]) || !j_config.RequirePrefix) {
				COMMAND * com = api->commands->FindCommand(cmd);
				if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
					USER_PRESENCE * up = alloc_jabber_presence(user, user ? user->Nick:from, hostmask2);
					hMutex.Release();
					api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE);
					hMutex.Lock();
					UnrefUser(up);
				}
			}

			if (user) { UnrefUser(user); }

			st.FreeString(cmd2);
			if (parms) { st.FreeString(parms); }
		}

		zfree(from);
	}

	return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

void SetStatus(bool available) {
	hMutex.Lock();
	if (j_config.connected) {
		LmMessage * m = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE, LM_MESSAGE_SUB_TYPE_AVAILABLE);
		g_print (":: %s\n", lm_message_node_to_string (m->node));
		lm_connection_send(j_config.connection, m, NULL);
		lm_message_unref(m);
	}
	hMutex.Release();
}

THREADTYPE JabberThread(void * lpData) {
	TT_THREAD_START

	j_config.lastRecv = time(NULL);
	time_t nextTry = time(NULL);

	j_config.connection = lm_connection_new(j_config.server);
	lm_connection_set_port(j_config.connection, j_config.port);

	LmMessageHandler * handler = NULL;
	if (j_config.connection) {
		handler = lm_message_handler_new(Jabber_CB, NULL, NULL);
		lm_connection_register_message_handler(j_config.connection, handler, LM_MESSAGE_TYPE_MESSAGE, LM_HANDLER_PRIORITY_NORMAL);
	}


	GMainLoop * main_loop = g_main_loop_new(NULL, FALSE);
	while (j_config.connection && !j_config.shutdown_now) {
		hMutex.Lock();

		if (j_config.reconnect) {
			lm_connection_close(j_config.connection, NULL);
			j_config.connected = j_config.reconnect = false;
			nextTry = time(NULL)+30;
		}

		if (!j_config.connected && time(NULL) >= nextTry) {
			GError * error = NULL;
			api->ib_printf("Jabber -> Connecting to Jabber server...\n");
			if (lm_connection_open_and_block(j_config.connection, &error)) {
				if (lm_connection_authenticate_and_block(j_config.connection, j_config.user, j_config.pass, j_config.resource, &error)) {
					j_config.connected = true;
					api->ib_printf("Jabber -> Connected to Jabber server!\n");
					SetStatus(true);
				} else {
					api->ib_printf("Jabber -> Failed to authenticate: %s\n", error->message);
					lm_connection_close(j_config.connection, NULL);
				}
			} else {
				api->ib_printf("Jabber -> Failed to open: %s\n", error->message);
			}
			j_config.lastRecv = time(NULL);
			nextTry = time(NULL)+30;
		} else if (j_config.connected) {
			if (!lm_connection_is_open(j_config.connection) || !lm_connection_is_authenticated(j_config.connection)) {
				api->ib_printf("Jabber -> Connection failed, reconnecting in 30 seconds...\n");
				j_config.reconnect = true;
			}
		}

		while (g_main_context_iteration(g_main_loop_get_context(main_loop), FALSE)) {;}
		safe_sleep(100,true);

	}

	hMutex.Lock();
	j_config.reconnect = j_config.connected = false;
	if (j_config.connection) {
		if (lm_connection_is_open(j_config.connection)) {
			api->ib_printf("Jabber -> Closing server connection...\n");
			lm_connection_close(j_config.connection, NULL);
		}
		if (handler) {
			lm_connection_unregister_message_handler(j_config.connection, handler, LM_MESSAGE_TYPE_MESSAGE);
			lm_message_handler_unref(handler);
		}
		lm_connection_unref(j_config.connection);
		j_config.connection = NULL;
	}
	hMutex.Release();

	g_main_loop_unref(main_loop);

	TT_THREAD_END
}


int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
		case IB_INIT:{
				memset(&j_config, 0, sizeof(j_config));
				j_config.MinLevel = 3;
				j_config.port = 5222;
				strcpy(j_config.resource, "/ircbot");

				DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Jabber");
				if (sec) {
					j_config.RequirePrefix = api->config->GetConfigSectionLong(sec, "RequirePrefix") > 0 ? true:false;
					int ml = api->config->GetConfigSectionLong(sec, "MinLevel");
					if (ml >= 1 && ml <= 5) {
						j_config.MinLevel = ml;
					}
					ml = api->config->GetConfigSectionLong(sec, "Port");
					if (ml >= 1 && ml <= 65535) {
						j_config.port = ml;
					}
                                        char * p = api->config->GetConfigSectionValue(sec, "Server");
                                        if (p) {
                                                strcpy(j_config.server, p);
                                        }
                                        p = api->config->GetConfigSectionValue(sec, "User");
                                        if (p) {
                                                strcpy(j_config.user, p);
                                        }
                                        p = api->config->GetConfigSectionValue(sec, "Pass");
                                        if (p) {
                                                strcpy(j_config.pass, p);
                                        }
                    p = api->config->GetConfigSectionValue(sec, "Resource");
                    if (p) {
                            strcpy(j_config.resource, p);
	                 }
				}

                                if (strlen(j_config.server) && strlen(j_config.user)) {
                                        TT_StartThread(JabberThread, NULL, "Jabber Thread", pluginnum);
                                } else {
                                        api->ib_printf("Jabber -> You must configure the Jabber plugin before using. See http://wiki.shoutirc.com/index.php/Configuration#Jabber for details\n");
                                }
			}
			break;

		case IB_SHUTDOWN:
			j_config.shutdown_now = true;
			api->ib_printf("Jabber -> Shutting down...\n");
			while (TT_NumThreadsWithID(pluginnum)) {
				safe_sleep(100,true);
			}
			api->ib_printf("Jabber -> Shut down.\n");
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{B3C3E2FB-FCE7-4AE9-9AEF-E15BA96C0AC8}",
	"Jabber",
	"Jabber/XMPP Plugin 1.0",
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

