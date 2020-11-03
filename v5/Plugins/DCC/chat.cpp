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

#include "dcc.h"

int CommandOutput_DCC(T_SOCKET * sock, const char * dest, const char * str) {
	sendMutex.Lock();
	static char buf[8192];
	sprintf(buf, "%s\r\n", str);
	int n = sockets->Send(sock, buf);
	sendMutex.Release();
	return n;
}

typedef void (*dcc_handler)(DCCT_CHAT * trans);

void chat_handler(DCCT_CHAT * trans) {
	while (trans->status == DS_AUTHENTICATED && !dcc_config.shutdown_now) {
		if (sockets->Select_Read(trans->sock, 1000) == 1) {
			char buf[1024];
			int n = sockets->RecvLine(trans->sock,buf,sizeof(buf));
			switch(n) {
				case RL3_ERROR:
					api->ib_printf(_("DCC Session Error: %s\n"), sockets->GetLastErrorString());
					trans->status = DS_DROP;//drop client
					continue;
					break;
				case RL3_CLOSED:
					api->ib_printf(_("DCC Session Closed\n"));
					trans->status = DS_DROP;//drop client
					continue;
					break;
				case RL3_NOLINE:
				case 0:
					api->safe_sleep_milli(100);
					continue;
					break;
			}

			buf[n]=0;
			strtrim(buf, "\r\n ");
			size_t len = strlen(buf);
			char *cmd2 = NULL;
			char *cmd = strtok_r(buf, " ",&cmd2);
			char *parms = NULL;
			if (len > strlen(cmd)) {
				parms = cmd+strlen(cmd)+1;
			}

			COMMAND * Scan = api->commands->FindCommand(cmd);
			if (Scan) {
				if (Scan->proc_type == COMMAND_PROC && (Scan->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
					USER_PRESENCE * ut = alloc_dcc_chat_presence(trans);
					api->commands->ExecuteProc(Scan, parms, ut, CM_ALLOW_CONSOLE_PRIVATE);
					UnrefUser(ut);
				}
			} else {
				CommandOutput_DCC(trans->sock,"",_("Unknown command"));
			}
		}
	}
}

void auth_handler(DCCT_CHAT * trans) {
	char buf[1024];
	if (trans->status == DS_CONNECTED) {
		while (trans->status == DS_CONNECTED && !dcc_config.shutdown_now) {
			if (sockets->Select_Read(trans->sock, 1000) == 1) {
				int n = sockets->RecvLine(trans->sock,buf,sizeof(buf));
				switch(n) {
					case RL3_ERROR:
						api->ib_printf(_("DCC Session Error: %s\n"), sockets->GetLastErrorString());
						trans->status = DS_DROP;//drop client
						continue;
						break;
					case RL3_CLOSED:
						api->ib_printf(_("DCC Session Closed\n"));
						trans->status = DS_DROP;//drop client
						continue;
						break;
					case RL3_NOLINE:
					case 0:
						api->safe_sleep_milli(100);
						continue;
						break;
				}

				buf[n]=0;
				strtrim(buf, "\r\n ");

				if (strlen(trans->User->Pass) && !stricmp(buf,trans->User->Pass)) {
					CommandOutput_DCC(trans->sock,"","");
					CommandOutput_DCC(trans->sock,""," ## #            #  ### ##   ##     ##       #  ");
					CommandOutput_DCC(trans->sock,"","#   ### ### # # ###  #  # # #       # # ### ### ");
					CommandOutput_DCC(trans->sock,""," #  # # # # # #  #   #  ##  #       ##  # #  #  ");
					CommandOutput_DCC(trans->sock,"","  # # # ### ###  ##  #  # # #       # # ###  ## ");
					CommandOutput_DCC(trans->sock,"","##                  ### # #  ##     ##          \r\n");

					trans->status = DS_AUTHENTICATED;
				} else {
					CommandOutput_DCC(trans->sock,"",_("Access Denied!"));
					trans->status = DS_DROP;//drop client
				}
			}
		}
	}

	if (trans->status == DS_AUTHENTICATED) {
		time_t tme=0;
		time(&tme);
		tm tml;
		localtime_r(&tme, &tml);
		char ib_time[64];
		strftime(ib_time, sizeof(ib_time), "%I:%M:%S %p", &tml);

		char buf2[64];
		api->user->uflag_to_string(trans->User->Flags, buf2, sizeof(buf2));
		sprintf(buf,_("Welcome %s, your user flags are '%s'!\r\nMy local time is: %s\r\n"),trans->User->Nick,buf2,ib_time);
		CommandOutput_DCC(trans->sock,"",buf);
		CommandOutput_DCC(trans->sock,"",_("Type \002commands\002 for a command list"));
		CommandOutput_DCC(trans->sock,"",_("Type \002help\002 for some basic help"));
		CommandOutput_DCC(trans->sock,"",_("Type \002help command\002 for help with a specific command"));
	}
}

void do_dcc_chat_session(DCCT_CHAT * trans, T_SOCKET * sock) {
	char buf[256];
	sprintf(buf,_("You are connected to %s, running RadioBot %s/%s\r\nThis sytem is for authorized users ONLY!\r\n\r\n"),api->irc->GetCurrentNick(-1),api->GetVersionString(),api->platform);
	trans->status = DS_CONNECTED;
	CommandOutput_DCC(sock,"",buf);
	//int n=0;
	//dcc_handler handler = auth_handler;
	while (trans->status >= DS_CONNECTED && !dcc_config.shutdown_now) {
		if (trans->status == DS_AUTHENTICATED) {
			chat_handler(trans);
		} else {
			CommandOutput_DCC(trans->sock, "", _("Enter your password:"));
			auth_handler(trans);
		}
	}
	if (dcc_config.shutdown_now && trans->status >= DS_CONNECTED) {
		CommandOutput_DCC(sock,"",_("RadioBot is shutting down, closing DCC Chat session..."));
	}
}

THREADTYPE ChatThread_Incoming(VOID *lpData) {
	TT_THREAD_START
	DCCT_CHAT * trans = (DCCT_CHAT*)tt->parm;
	char buf[256],buf2[64];
	trans->status = DS_CONNECTING;
	//int status=0;

	api->user->uflag_to_string(trans->Pres->Flags, buf2, sizeof(buf2));
	sprintf(buf,_("[IRC-%d] Accepting DCC Chat from %s[%s]..."),trans->Pres->NetNo,trans->Pres->Nick,buf2);
	api->irc->LogToChan(buf);
	api->ib_printf(_("[IRC-%d] Accepting DCC Chat from %s[%s]...\n"),trans->Pres->NetNo,trans->Pres->Nick,buf2);
	T_SOCKET * sock = sockets->Create();
	sendMutex.Lock();
	trans->sock = sock;
	chatList.push_back(trans);
	sendMutex.Release();
	if (sockets->ConnectWithTimeout(sock,trans->ip,trans->port,10000)) {
		do_dcc_chat_session(trans, sock);
	} else {
		api->ib_printf(_("DCC Support -> Error connecting to DCC Chat for user %s at %s:%d\n"), trans->Pres->Nick, trans->ip, trans->port);
	}

	sendMutex.Lock();
	std::vector<DCCT_CHAT *>::iterator x = chatList.begin();
	for (; x != chatList.end(); x++) {
		if (*x == trans) {
			chatList.erase(x);
			break;
		}
	}
	sockets->Close(sock);
	sendMutex.Release();

	UnrefUser(trans->Pres);
	zfree(trans);
	TT_THREAD_END
}

THREADTYPE ChatThread_Outgoing(VOID *lpData) {
	TT_THREAD_START
	DCCT_CHAT * trans = (DCCT_CHAT*)tt->parm;
	char buf[256],buf2[64],local_ip[128];
	trans->status = DS_CONNECTING;

	sstrcpy(local_ip, strlen(dcc_config.local_ip) ? dcc_config.local_ip:api->GetIP());

	sendMutex.Lock();
	chatList.push_back(trans);
	sendMutex.Release();

	api->user->uflag_to_string(trans->Pres->Flags, buf2, sizeof(buf2));
	snprintf(buf,sizeof(buf),_("[IRC-%d] Offering DCC Chat to <%s[%s]>..."),trans->Pres->NetNo,trans->Pres->Nick,buf2);
	api->irc->LogToChan(buf);
	api->ib_printf("%s\n",buf);

	snprintf(buf, sizeof(buf), "NOTICE %s :DCC Chat (%s)\r\n", trans->Pres->Nick, local_ip);
	api->irc->SendIRC(trans->Pres->NetNo, buf, strlen(buf));
//	uint32 longip = ntohl(inet_addr(local_ip));
	struct in_addr xx;
	inet_pton(AF_INET, local_ip, &xx);
	uint32 longip = ntohl(xx.s_addr);
	snprintf(buf, sizeof(buf), "PRIVMSG %s :\001DCC CHAT chat %u %d\001\r\n", trans->Pres->Nick, longip, trans->lSock->local_port);
	api->irc->SendIRC(trans->Pres->NetNo, buf, strlen(buf));

	T_SOCKET * sock = NULL;
	if (WaitXSecondsForSock(trans->lSock, 60*3) == 1) {
		sock = sockets->Accept(trans->lSock);
		if (sock) {
			api->user->uflag_to_string(trans->Pres->Flags, buf2, sizeof(buf2));
			sprintf(buf,_("[IRC-%d] <%s[%s]> connected to Chat..."),trans->Pres->NetNo,trans->Pres->Nick,buf2);
			api->irc->LogToChan(buf);
			api->ib_printf(_("[IRC-%d] <%s[%s]> connected to Chat...\n"),trans->Pres->NetNo,trans->Pres->Nick,buf2);
			sockets->Close(trans->lSock);
			trans->lSock = NULL;

			sendMutex.Lock();
			sstrcpy(trans->ip, sock->remote_ip);
			trans->port = sock->remote_port;
			trans->sock = sock;
			trans->status = DS_CONNECTED;
			sendMutex.Release();

			do_dcc_chat_session(trans, sock);
		} else {
			api->ib_printf(_("Error accepting connection for %s to connect to FServ!\n"), trans->Pres->Nick);
		}
	} else {
		api->ib_printf(_("Timeout waiting for %s to connect to FServ!\n"), trans->Pres->Nick);
	}
	if (trans->lSock) { sockets->Close(trans->lSock); trans->lSock = NULL; }

	sendMutex.Lock();
	std::vector<DCCT_CHAT *>::iterator x = chatList.begin();
	for (; x != chatList.end(); x++) {
		if (*x == trans) {
			chatList.erase(x);
			break;
		}
	}
	if (sock) { sockets->Close(sock); }
	sendMutex.Release();

	UnrefUser(trans->Pres);
	zfree(trans);
	TT_THREAD_END
}

static char dcc_chat_desc[] = "DCC Chat";

#if !defined(DEBUG)
void dcc_chat_up_ref(USER_PRESENCE * u) {
#else
void dcc_chat_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("dcc_chat_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	sendMutex.Lock();
	RefUser(u->User);
	u->ref_cnt++;
	sendMutex.Release();
}

#if !defined(DEBUG)
void dcc_chat_up_unref(USER_PRESENCE * u) {
#else
void dcc_chat_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("dcc_chat_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	sendMutex.Lock();
	UnrefUser(u->User);
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("dcc_chat_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u);
	}
	sendMutex.Release();
}

bool send_dcc_chat_pm(USER_PRESENCE * ut, const char * str) {
	sendMutex.Lock();
	static char buf[8192];
	sprintf(buf, "%s\r\n", str);
	int n = sockets->Send(ut->Sock, buf);
	sendMutex.Release();
	return n > 0 ? true:false;
}

USER_PRESENCE * alloc_dcc_chat_presence(USER_PRESENCE * ut, T_SOCKET * sock) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = ut->User;
	ret->Sock = sock;
	ret->Hostmask = zstrdup(ut->Hostmask);
	ret->Nick = zstrdup(ret->User->Nick);
	ret->NetNo = -1;
	ret->Flags = ret->User->Flags;

	ret->send_chan_notice = send_dcc_chat_pm;
	ret->send_chan = send_dcc_chat_pm;
	ret->send_msg = send_dcc_chat_pm;
	ret->send_notice = send_dcc_chat_pm;
	ret->std_reply = send_dcc_chat_pm;
	ret->Desc = dcc_chat_desc;

	ret->ref = dcc_chat_up_ref;
	ret->unref = dcc_chat_up_unref;

	RefUser(ret);
	return ret;
};

USER_PRESENCE * alloc_dcc_chat_presence(DCCT_CHAT * trans) {
	return alloc_dcc_chat_presence(trans->Pres, trans->sock);
};
