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

#include "sms.h"
#ifndef USE_SSL
#define USE_SSL
#endif
#include <libspopc.h>

#if defined(WIN32)
	#if defined(DEBUG)
	#pragma comment(lib, "libspopc_d.lib")
	#else
	#pragma comment(lib, "libspopc.lib")
	#endif
#endif

msgList inMessages;
Titus_Mutex hMutex;
static char sms_desc[] = "SMS";

#if !defined(DEBUG)
void sms_up_ref(USER_PRESENCE * u) {
#else
void sms_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("sms_UserPres_AddRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	hMutex.Lock();
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	hMutex.Release();
}

#if !defined(DEBUG)
void sms_up_unref(USER_PRESENCE * u) {
#else
void sms_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("sms_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	hMutex.Lock();
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("sms_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u->Ptr1);
		zfree(u);
	}
	hMutex.Release();
}

bool send_sms_pm(USER_PRESENCE * ut, const char * str) {
	hMutex.Lock();
	if (sms_config.enabled) {
		Send_SMS((const char *)ut->Ptr1, str);
	}
	hMutex.Release();
	return true;
}

USER_PRESENCE * alloc_sms_presence(const char * phone) {
	hMutex.Lock();
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	if (sms_config.HostmaskForm == 1) {
		ret->Nick = zstrdup("SMS");
		ret->Hostmask = zmprintf("SMS!%s@sms", phone);
	} else {
		ret->Nick = zstrdup(phone);
		ret->Hostmask = zmprintf("%s!sms@sms", phone);
	}
	ret->User = api->user->GetUser(ret->Hostmask);
	ret->NetNo = -1;
	ret->Flags = ret->User ? ret->User->Flags:api->user->GetDefaultFlags();
	ret->Ptr1 = zstrdup(phone);

	ret->send_chan_notice = send_sms_pm;
	ret->send_chan = send_sms_pm;
	ret->send_msg = send_sms_pm;
	ret->send_notice = send_sms_pm;
	ret->std_reply = send_sms_pm;
	ret->Desc = sms_desc;

	ret->ref = sms_up_ref;
	ret->unref = sms_up_unref;

	RefUser(ret);
	hMutex.Release();
	return ret;
};

TT_DEFINE_THREAD(POP_Thread) {
	TT_THREAD_START
	libspopc_init();
	char buf[160];
	while (!sms_config.shutdown_now) {
		popsession * ses = NULL;
		api->ib_printf("SMS -> Checking mail...\n");
		if (sms_config.pop3.ssl == 1) {
			pop3_ssl_always();
		} else if (sms_config.pop3.ssl == 0) {
			pop3_ssl_never();
		} else {
			pop3_ssl_auto();
		}
		char * tmp = zmprintf("%s:%d", sms_config.pop3.host, sms_config.pop3.port);
		char * err = popbegin(tmp, sms_config.pop3.user, sms_config.pop3.pass, &ses);
		zfree(tmp);
		if (err == NULL) {
			api->ib_printf("SMS -> Inbox has %d messages in %d bytes.\n",popnum(ses), popbytes(ses));
			for(int i=1;i<=poplast(ses);i++){
				SMS_Message msg;

				err=popgethead(ses,i);
				if (err) {
					int len = strlen(err)+1;
					char * tmp = zstrdup(err);
					str_replace(tmp, len, "\r", "\n");
					while (strstr(tmp, "\n\n")) {
						str_replace(tmp, len, "\n\n", "\n");
					}
					StrTokenizer st(tmp, '\n');
					for (long j=1; j <= st.NumTok(); j++) {
						char * line = st.GetSingleTok(j);
						if (!strnicmp(line, "Subject:",8)) {
							char * p = line + 8;
							strtrim(p);
							if (!strnicmp(p, "SMS from ",9)) {
								p += 9;
								len = strlen(p)+1;
								str_replace(p, len, "(", "");
								str_replace(p, len, ")", "");
								str_replace(p, len, "-", "");
								while (strchr(p, ' ')) {
									str_replace(p, len, " ", "");
								}
								strtrim(p);
								api->ib_printf("SMS -> Got message from %s\n", p);
								msg.from = p;
							}
						}
						st.FreeString(line);
					}
					zfree(tmp);
					free(err);

					if (msg.from.length()) {
						err=popgetmsg(ses,i);
						if (err) {
							//int len = strlen(err)+1;
							tmp = zstrdup(err);
							char * p2 = NULL;
							char * p = strtok_r(tmp, "\n", &p2);
							//StrTokenizer st(tmp, '\n');
							bool is_next = false;
							while (p) {
								strtrim(p);
							//for (long j=1; j <= st.NumTok(); j++) {
								/*
								if (st.stdGetSingleTok(j).length() == 0) {
									is_next = true;
								} else if (is_next) {
									char * tmp2 = st.GetSingleTok(j);
									strtrim(tmp2);
									msg.message = tmp2;
									st.FreeString(tmp2);
									break;
								}
								*/
								if (*p == 0) {
									is_next = true;
								} else if (is_next) {
									msg.message = p;
									//st.FreeString(tmp2);
									break;
								}
								p = strtok_r(NULL, "\n", &p2);
							}
							if (msg.message.length()) {
								api->ib_printf("SMS -> Message: %s\n", msg.message.c_str());
								inMessages.push_back(msg);
							} else {
								api->ib_printf("SMS -> Error parsing message text!\n");
							}
							zfree(tmp);
							free(err);
							popdelmsg(ses, i);
						} else {
							api->ib_printf("SMS -> Error retreiving message text!\n");
						}
					}

					//printf("message %d (%s):\n%s",i,popmsguid(ses,i),err);
				} else {
					api->ib_printf("SMS -> Error retreiving message header for message %d\n", i);
				}
				//free(err);err=NULL;
			}
			api->ib_printf("SMS -> Disconnecting from POP3 server...\n");
			popend(ses);
		} else {
			api->ib_printf("SMS -> Error logging in to POP3 server: %s\n", err);
			free(err);
		}

		for (msgList::const_iterator x = inMessages.begin(); x != inMessages.end(); x++) {
			USER_PRESENCE * up = alloc_sms_presence(x->from.c_str());

			SMS_MESSAGE msg;
			memset(&msg, 0, sizeof(msg));
			msg.pres = up;
			msg.phone = x->from.c_str();
			msg.text = x->message.c_str();
			if (api->SendMessage(-1, SMS_ON_RECEIVED, (char *)&msg, sizeof(msg)) == 0) {
				StrTokenizer st((char *)x->message.c_str(), ' ');
				char * cmd2 = st.GetSingleTok(1);
				char * cmd = cmd2;
				char * parms = NULL;
				if (st.NumTok() > 1) {
					parms = st.GetTok(2,st.NumTok());
				}

				if (api->commands->IsCommandPrefix(cmd[0])) {
					cmd++;
				}

				bool didCommand = false;
				if (api->commands->IsCommandPrefix(cmd2[0]) || !sms_config.RequirePrefix) {
					COMMAND * com = api->commands->FindCommand(cmd);
					if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
						didCommand = true;
						api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE|CM_FLAG_SLOW);
					}
				}

				if (!didCommand && api->LoadMessage("SMS_InvalidCommand", buf, sizeof(buf))) {
					str_replace(buf, sizeof(buf), "%nick", up->User ? up->User->Nick:up->Nick);
					str_replace(buf, sizeof(buf), "%number", x->from.c_str());
					api->ProcText(buf, sizeof(buf));
					up->send_msg(up, buf);
				}

				if (parms) { st.FreeString(parms); }
				st.FreeString(cmd2);
			}
			UnrefUser(up);
		}
		inMessages.clear();

		for (int i=0; i < 60 && !sms_config.shutdown_now; i++) {
			safe_sleep(1);
		}
	}
	libspopc_clean();
	TT_THREAD_END
}

