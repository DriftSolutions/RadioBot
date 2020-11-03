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

#include "Telnet.h"

TelnetSession::TelnetSession(T_SOCKET * sSock) {
	User = NULL;
	Pres = NULL;
	status = DS_CONNECTED;
	bufind = 0;
	memset(buf, 0, sizeof(buf));
	ctrl = 0;
	console = false;
	username = "";
	timestamp = time(NULL);
	sock = sSock;
}
TelnetSession::~TelnetSession() {
	if (sock) {
		sockets->Close(sock);
		sock = NULL;
	}
}

int TelnetSend(T_SOCKET * sock, const char * str) {
	chatMutex.Lock();
	static char buf[8192];
	snprintf(buf, sizeof(buf), "%s", str);
	int n = sockets->Send(sock, buf);
	chatMutex.Release();
	return n;
}

bool do_telnet_always(TelnetSession * ses, char * buf) {
	if (!stricmp(buf, "STARTTLS")) {
		if (sockets->IsEnabled(TS3_FLAG_SSL)) {
			TelnetSend(ses->sock, "TLS_ENABLE\r\n");
			if (!sockets->SwitchToSSL_Server(ses->sock)) {
				TelnetSend(ses->sock, "TLS_ERROR Error in SSL handshake\r\n");
				ses->status = DS_DROP;
			}
		} else {
			TelnetSend(ses->sock, "TLS_ERROR Unsupported\r\n");
		}
		return true;
	}
	if (!stricmp(buf, "exit") || !stricmp(buf, "quit")) {
		TelnetSend(ses->sock, "Goodbye...\r\n");
		ses->status = DS_DROP;
		return true;
	}
	return false;
}

void do_telnet_welcome(TelnetSession * ses) {
	char buf[256];
	sprintf(buf,_("You are connected to %s, running RadioBot %s/%s\r\nThis sytem is for authorized users ONLY!\r\n\r\n"),api->GetIP()?api->GetIP():"unknown",api->GetVersionString(),api->platform);
	ses->status = DS_CONNECTED;
	TelnetSend(ses->sock,buf);
	TelnetSend(ses->sock, _("Enter your username: "));
}

void do_telnet_get_user(TelnetSession * ses, char * buf) {
	ses->username = buf;
	TelnetSend(ses->sock, _("Enter your password: "));
	ses->status = DS_NEED_PASS;
}

void do_telnet_get_pass(TelnetSession * ses, char * buf) {
	if (api->user->GetUserCount() == 0) {
		TelnetSend(ses->sock,"First login, creating your admin account... ");
		USER * tmp = api->user->AddUser(ses->username.c_str(), buf, api->user->GetLevelFlags(1), false);
		if (tmp) {
			UnrefUser(tmp);
			TelnetSend(ses->sock,"OK\r\n");
		} else {
			TelnetSend(ses->sock,"ERROR! Make sure your username doesn't include any invalid characters!\r\n");
		}
	}
	USER * u = api->user->GetUserByNick(ses->username.c_str());
	if (u && u->Pass[0] && !strcmp(u->Pass, buf)) {
		ses->User = u;
		ses->Pres = alloc_telnet_presence(ses);
		ses->status = DS_AUTHENTICATED;

		TelnetSend(ses->sock,"\r\n");
		TelnetSend(ses->sock," _____           _ _       ____        _   \r\n");
		TelnetSend(ses->sock,"|  __ \\         | (_)     |  _ \\      | |  \r\n");
		TelnetSend(ses->sock,"| |__) |__ _  __| |_  ___ | |_) | ___ | |_ \r\n");
		TelnetSend(ses->sock,"|  _  // _` |/ _` | |/ _ \\|  _ < / _ \\| __|\r\n");
		TelnetSend(ses->sock,"| | \\ \\ (_| | (_| | | (_) | |_) | (_) | |_ \r\n");
		TelnetSend(ses->sock,"|_|  \\_\\__,_|\\__,_|_|\\___/|____/ \\___/ \\__|\r\n\r\n");
		/*
		if (!(api->GetVersion() & IRCBOT_VERSION_FLAG_STANDALONE)) {
			TelnetSend(ses->sock,"\r\n");
			TelnetSend(ses->sock," ## #            #  ### ##   ##     ##       #  \r\n");
			TelnetSend(ses->sock,"#   ### ### # # ###  #  # # #       # # ### ### \r\n");
			TelnetSend(ses->sock," #  # # # # # #  #   #  ##  #       ##  # #  #  \r\n");
			TelnetSend(ses->sock,"  # # # ### ###  ##  #  # # #       # # ###  ## \r\n");
			TelnetSend(ses->sock,"##                  ### # #  ##     ##          \r\n\r\n");
		} else {
			TelnetSend(ses->sock,"\r\n");
			TelnetSend(ses->sock,"   ___        _       ______  ___  \r\n");
			TelnetSend(ses->sock,"  / _ \\      | |      |  _  \\|_  | \r\n");
			TelnetSend(ses->sock," / /_\\ \\_   _| |_ ___ | | | |  | | \r\n");
			TelnetSend(ses->sock," |  _  | | | | __/ _ \\| | | |  | | \r\n");
			TelnetSend(ses->sock," | | | | |_| | || (_) | |/ /\\__/ / \r\n");
			TelnetSend(ses->sock," \\_| |_/\\__,_|\\__\\___/|___/\\____/  \r\n\r\n");
		}
		*/

		time_t tme=0;
		time(&tme);
		tm tml;
		localtime_r(&tme, &tml);
		char ib_time[64];
		strftime(ib_time, sizeof(ib_time), "%I:%M:%S %p", &tml);

		char buf2[64];
		api->user->uflag_to_string(u->Flags, buf2, sizeof(buf2));
		snprintf(buf,TELNET_BUFFER_SIZE,_("Welcome %s, your user flags are '%s'!\r\nMy local time is: %s\r\n\r\n"),u->Nick,buf2,ib_time);
		TelnetSend(ses->sock,buf);
		TelnetSend(ses->sock,_("Type 'commands' for a command list\r\n"));
		TelnetSend(ses->sock,_("Type 'help' for some basic help\r\n"));
		TelnetSend(ses->sock,_("Type 'help command' for help with a specific command\r\n"));
		TelnetSend(ses->sock,_("Type 'quit' or 'exit' to disconnect\r\n\r\n"));
		TelnetSend(ses->sock,"> ");
		//char buf2[64];
		//api->user->uflag_to_string(u->Flags, buf2, sizeof(buf2));
		//snprintf(buf, TELNET_BUFFER_SIZE, "You are now logged in! Welcome, %s. You have flags: %s\r\n", u->Nick, buf2);
		//TelnetSend(ses->sock, buf);
	} else {
		TelnetSend(ses->sock, "Invalid username or password!\r\n");
		if (u) { UnrefUser(u); }
		ses->status = DS_DROP;
	}
}

void do_telnet_cmd(TelnetSession * ses, char * buf, size_t len) {
	char *cmd2 = NULL;
	char *cmd = strtok_r(buf, " ",&cmd2);
	char *parms = NULL;
	if (len > strlen(cmd)) {
		parms = cmd+strlen(cmd)+1;
	}

	if (api->commands->IsCommandPrefix(cmd[0])) { cmd++; }
	COMMAND * Scan = api->commands->FindCommand(cmd);
	if (Scan) {
		if (Scan->proc_type == COMMAND_PROC && (Scan->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
			api->commands->ExecuteProc(Scan, parms, ses->Pres, CM_ALLOW_CONSOLE_PRIVATE);
		} else {
			TelnetSend(ses->sock,_("Invalid command type\r\n"));
		}
	} else if (!stricmp(cmd, "cons") && api->user->uflag_have_any_of(ses->User->Flags, UFLAG_MASTER)) {
		ses->console = ses->console ? false:true;
		if (ses->console) {
			TelnetSend(ses->sock,_("Bot console output enabled\r\n"));
		} else {
			TelnetSend(ses->sock,_("Bot console output disabled\r\n"));
		}
	} else {
		TelnetSend(ses->sock,_("Unknown command\r\n"));
	}
	TelnetSend(ses->sock,"> ");
}

void handle_session(TelnetSession * ses) {
	static unsigned char buf[TELNET_BUFFER_SIZE];
	int n = sockets->Recv(ses->sock,(char *)buf,sizeof(buf));
	switch(n) {
		case -1:
			api->ib_printf(_("Telnet Session Error: %s\n"), sockets->GetLastErrorString());
			ses->status = DS_DROP;//drop client
			return;
			break;
		case 0:
			api->ib_printf(_("Telnet Session Closed\n"));
			ses->status = DS_DROP;//drop client
			return;
			break;
	}

	for (int i=0; i < n; i++) {
		if (buf[i] == 0xFF) {
			ses->ctrl = 1;
		} else if (ses->ctrl > 0) {
			if (ses->ctrl == 1) {
				if (buf[i] >= 251 && buf[i] <= 254) {
					ses->ctrl = 2;
				} else if (buf[i] == 0xFF) {
					ses->buf[ses->bufind++] = buf[i];
					ses->ctrl = 0;
				} else {
					buf[i] = buf[i];
					ses->ctrl = 0;
				}
			} else if (ses->ctrl == 2) {
				ses->ctrl = 0;
			}
		} else if (buf[i] == '\r') {
			//ignore it
		} else if (buf[i] == '\n' || buf[i] == 0) {
			//bool was_n = (buf[i] == '\n') ? true:false;
			ses->buf[ses->bufind] = 0;
			strtrim(ses->buf, "\r\n ");
			size_t len = strlen(ses->buf);
			ses->timestamp = time(NULL);
			if (len > 0) {
				if (!do_telnet_always(ses, ses->buf)) {
					if (ses->status == DS_CONNECTED) {
						do_telnet_get_user(ses, ses->buf);
					} else if (ses->status == DS_NEED_PASS) {
						do_telnet_get_pass(ses, ses->buf);
					} else if (ses->status == DS_AUTHENTICATED) {
						ses->Pres->Flags = ses->User->Flags;
						do_telnet_cmd(ses, ses->buf, len);
					}
				}
			} else if (ses->status == DS_AUTHENTICATED) {
				TelnetSend(ses->sock,"> ");
			}
			ses->bufind = 0;
		} else {
			ses->buf[ses->bufind++] = buf[i];
		}
		if (ses->bufind >= TELNET_BUFFER_SIZE) {
			TelnetSend(ses->sock, "Line too long, dropping...\r\n");
			ses->bufind = 0;
		}
	}
}

TT_DEFINE_THREAD(ChatThread) {
	TT_THREAD_START
	//char buf[1024];
	TITUS_SOCKET_LIST tfd;
	while (!telconf.shutdown_now) {
		TFD_ZERO(&tfd);
		TFD_SET(&tfd, telconf.lSock);
		chatMutex.Lock();
		for (chatListType::const_iterator x = sessions.begin(); x != sessions.end(); x++) {
			TFD_SET(&tfd, (*x)->sock);
		}
		chatMutex.Release();
		int n = sockets->Select_Read_List(&tfd, 1000);
		if (n > 0) {
			if (TFD_ISSET(&tfd, telconf.lSock)) {
				T_SOCKET * sock = sockets->Accept(telconf.lSock);
				if (sock) {
					sockets->SetKeepAlive(sock, true);
					//sockets->SetRecvTimeout(sock, 30000);
					sockets->SetSendTimeout(sock, 30000);
					chatMutex.Lock();
					if (sessions.size() < telconf.MaxSessions) {
						TelnetSession * ses = new TelnetSession(sock);
						sessions.push_back(ses);
						chatMutex.Release();
						api->ib_printf("Telnet -> Accepted connection from %s:%u\n", sock->remote_ip, sock->remote_port);
						do_telnet_welcome(ses);
					} else {
						chatMutex.Release();
						api->ib_printf("Telnet -> Turned away connection from %s:%u (server is full)\n", sock->remote_ip, sock->remote_port);
						sockets->Send(sock, "Sorry, server is full!\r\n");
						sockets->Close(sock);
					}
				}
			}

			chatMutex.Lock();
			for (chatListType::iterator x = sessions.begin(); x != sessions.end(); x++) {
				if (TFD_ISSET(&tfd, (*x)->sock)) {
					handle_session(*x);
				}
			}
			chatMutex.Release();
		}
		chatMutex.Lock();
		for (chatListType::iterator x = sessions.begin(); x != sessions.end();) {
			if ((*x)->status == DS_DROP) {
				delete *x;
				sessions.erase(x);
				x = sessions.begin();
			} else if (((*x)->status < DS_AUTHENTICATED && (time(NULL)-(*x)->timestamp) > 30) || ((*x)->status == DS_AUTHENTICATED && (time(NULL)-(*x)->timestamp) > 1800)) {
				TelnetSend((*x)->sock, "Session timed out!\n");
				delete *x;
				sessions.erase(x);
				x = sessions.begin();
			} else {
				x++;
			}
		}
		chatMutex.Release();
	}
	chatMutex.Lock();
	sockets->Close(telconf.lSock);
	telconf.lSock = NULL;
	for (chatListType::iterator x = sessions.begin(); x != sessions.end(); x = sessions.begin()) {
		TelnetSend((*x)->sock, "Telnet plugin is shutting down...\n");
		delete *x;
		sessions.erase(x);
	}
	chatMutex.Release();
	TT_THREAD_END
}

static char telnet_chat_desc[] = "Telnet";

#if !defined(DEBUG)
void telnet_chat_up_ref(USER_PRESENCE * u) {
#else
void telnet_chat_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("telnet_chat_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	chatMutex.Lock();
	RefUser(u->User);
	u->ref_cnt++;
	chatMutex.Release();
}

#if !defined(DEBUG)
void telnet_chat_up_unref(USER_PRESENCE * u) {
#else
void telnet_chat_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("telnet_chat_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	chatMutex.Lock();
	UnrefUser(u->User);
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("telnet_chat_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree(u);
	}
	chatMutex.Release();
}

bool send_telnet_chat_pm(USER_PRESENCE * ut, const char * str) {
	chatMutex.Lock();
	static char buf[8192];
	snprintf(buf, sizeof(buf), "%s\r\n", str);
	int n = sockets->Send(ut->Sock, buf);
	chatMutex.Release();
	return n > 0 ? true:false;
}

USER_PRESENCE * alloc_telnet_presence(USER * user, T_SOCKET * sock) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = user;
	ret->Sock = sock;
	ret->Hostmask = zmprintf("%s!%s@%s", user->Nick, user->Nick, sock->remote_ip);
	ret->Nick = zstrdup(user->Nick);
	ret->NetNo = -1;
	ret->Flags = user->Flags;

	ret->send_chan_notice = send_telnet_chat_pm;
	ret->send_chan = send_telnet_chat_pm;
	ret->send_msg = send_telnet_chat_pm;
	ret->send_notice = send_telnet_chat_pm;
	ret->std_reply = send_telnet_chat_pm;
	ret->Desc = telnet_chat_desc;

	ret->ref = telnet_chat_up_ref;
	ret->unref = telnet_chat_up_unref;

	RefUser(ret);
	return ret;
};

USER_PRESENCE * alloc_telnet_presence(TelnetSession * trans) {
	return alloc_telnet_presence(trans->User, trans->sock);
};
