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

#include "skype_plugin.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
Titus_Mutex hMutex;
Titus_Sockets socks;
SKYPE_CONFIG skype_config;

bool status_cb(SKYPE_API_STATUS status) {
	//LockMutex(hMutex);
	switch(status) {
		case SAPI_STATUS_DISCONNECTED:
			api->ib_printf(_("Skype -> Disconnected from Skype API!\n"));
			if (!skype_config.shutdown_now) { skype_config.reconnect = true; }
			break;
		case SAPI_STATUS_CONNECTING:
			api->ib_printf(_("Skype -> Connecting to Skype API...\n"));
			break;
		case SAPI_STATUS_CONNECTED:
			api->ib_printf(_("Skype -> Skype API Connected!\n"));
			break;

		case SAPI_STATUS_PENDING_AUTHORIZATION:
			api->ib_printf(_("Skype -> Pending Authorization with Skype...\n"));
			break;
		case SAPI_STATUS_AVAILABLE:
			api->ib_printf(_("Skype -> Skype API Available!\n"));
			break;

		case SAPI_STATUS_ERROR:
		case SAPI_STATUS_REFUSED:
		case SAPI_STATUS_NOT_AVAILABLE:
		default:
			if (!skype_config.shutdown_now) { skype_config.reconnect = true; }
			break;
	}
	//RelMutex(hMutex);
	return true;
}

class MESSAGE {
public:
	std::string from, body, type;
	time_t ts;
};

typedef std::map<int32, MESSAGE> msgListType;
msgListType msgList;

/*
int SkypeOutput(T_SOCKET * sock, char * dest, char * str) {
	LockMutex(hMutex);
	static char buf[8192];
	if (skype_config.connected) {
		sprintf(buf, "MESSAGE %s %s", dest, str);
		SkypeAPI_SendCommand(buf);
	}
	RelMutex(hMutex);
	return 1;
}
*/

static char skype_desc[] = "Skype Chat";

#if !defined(DEBUG)
void skype_up_ref(USER_PRESENCE * u) {
#else
void skype_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("skype_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	RelMutex(hMutex);
}

#if !defined(DEBUG)
void skype_up_unref(USER_PRESENCE * u) {
#else
void skype_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("skype_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("skype_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u);
	}
	RelMutex(hMutex);
}

bool send_skype_pm(USER_PRESENCE * ut, const char * str) {
	LockMutex(hMutex);
	static char buf[8192];
	if (skype_config.connected) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf)-1, "MESSAGE %s %s", ut->Nick, str);
		SkypeAPI_SendCommand(buf);
	}
	RelMutex(hMutex);
	return true;
}

USER_PRESENCE * alloc_skype_presence(USER * user, const char * username, const char * hostmask) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = user;
	ret->Nick = zstrdup(username);
	ret->Hostmask = zstrdup(hostmask);
	ret->NetNo = -1;
	ret->Flags = user ? user->Flags:api->user->GetDefaultFlags();

	ret->send_chan_notice = send_skype_pm;
	ret->send_chan = send_skype_pm;
	ret->send_msg = send_skype_pm;
	ret->send_notice = send_skype_pm;
	ret->std_reply = send_skype_pm;
	ret->Desc = skype_desc;

	ret->ref = skype_up_ref;
	ret->unref = skype_up_unref;

	RefUser(ret);
	return ret;
};

void proc_msg(int32 id) {
	msgList[id].ts = time(NULL);
	if (msgList[id].body.length() && msgList[id].from.length() && msgList[id].type.length()) {
		if (!stricmp(msgList[id].type.c_str(),"SAID")) {
			char hostmask[1024];
			sprintf(hostmask, "%s!%s@skype.com", msgList[id].from.c_str(), msgList[id].from.c_str());
			USER * user = api->user->GetUser(hostmask);
			uint32 flags = user ? user->Flags:api->user->GetDefaultFlags();
			char buf[64];
			api->user->uflag_to_string(flags, buf, sizeof(buf));
			api->ib_printf(_("Skype -> Message from %s[%s]: %s\n"), msgList[id].from.c_str(), buf, msgList[id].body.c_str());
			//if (flags & UFLAG_SKYPE) {
				StrTokenizer st((char *)msgList[id].body.c_str(), ' ');
				char * cmd2 = st.GetSingleTok(1);
				char * cmd = cmd2;
				char * parms = NULL;
				if (st.NumTok() > 1) {
					parms = st.GetTok(2,st.NumTok());
				}

				if (api->commands->IsCommandPrefix(cmd[0])) {
					cmd++;
				}

				if (api->commands->IsCommandPrefix(cmd2[0]) || !skype_config.RequirePrefix) {
					COMMAND * com = api->commands->FindCommand(cmd);
					if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
						USER_PRESENCE * up = alloc_skype_presence(user, msgList[id].from.c_str(), hostmask);
						RelMutex(hMutex);
						api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE);
						LockMutex(hMutex);
						UnrefUser(up);
					}
				}
				if (parms) { st.FreeString(parms); }
				st.FreeString(cmd2);
			//}
			if (user) { UnrefUser(user); }
		}
		msgList.erase(id);
	}
}

bool message_cb(char * msg) {
	LockMutex(hMutex);
#if defined(DEBUG)
	api->ib_printf("Skype -> Incoming: %s\n", msg);
#endif
	skype_config.lastRecv = time(NULL);
	if (!stricmp(msg, "PONG")) {
		api->ib_printf(_("Skype -> Ping? Pong!\n"));
	}
	if (!strnicmp(msg, "CHATMESSAGE ", 12)) {
		StrTokenizer st(msg, ' ');
		if (st.NumTok() >= 4) {
			int32 id = atoi(st.stdGetSingleTok(2).c_str());
			std::string p = st.stdGetTok(3,4);
			if (!stricmp(p.c_str(), "STATUS RECEIVED")) {
				char buf[1024];
				sprintf(buf, "GET CHATMESSAGE %d BODY", id);
				SkypeAPI_SendCommand(buf);
				sprintf(buf, "GET CHATMESSAGE %d FROM_HANDLE", id);
				SkypeAPI_SendCommand(buf);
				sprintf(buf, "GET CHATMESSAGE %d TYPE", id);
				SkypeAPI_SendCommand(buf);
			}
			p = st.stdGetSingleTok(3);
			if (!stricmp(p.c_str(), "BODY")) {
				msgList[id].body = st.stdGetTok(4,st.NumTok());
				proc_msg(id);
			}
			if (!stricmp(p.c_str(), "FROM_HANDLE")) {
				msgList[id].from = st.stdGetSingleTok(4);
				proc_msg(id);
			}
			if (!stricmp(p.c_str(), "TYPE")) {
				msgList[id].type = st.stdGetSingleTok(4);
				proc_msg(id);
			}
		}
	}

	if (!strnicmp(msg, "CALL ", 5)) {
		StrTokenizer st(msg, ' ');
		if (st.NumTok() >= 4) {
			char buf[1024];
			int32 id = atoi(st.stdGetSingleTok(2).c_str());
			std::string p = st.stdGetSingleTok(3);

			if (!stricmp(p.c_str(), "STATUS")) {
				std::string q = st.stdGetSingleTok(4);

				if (!stricmp(q.c_str(), "RINGING")) {
					api->ib_printf(_("Skype -> Incoming Call!\n"));
					if (skype_config.AnswerMode == SAM_TRANSFER && strlen(skype_config.TransferCallsTo)) {
						sprintf(buf, "GET CALL %d CAN_TRANSFER %s", id, skype_config.TransferCallsTo);
						SkypeAPI_SendCommand(buf);
					} else if (skype_config.AnswerMode == SAM_AUTO_ANSWER) {
						api->ib_printf(_("Skype -> Answering call...\n"));
						sprintf(buf, "ALTER CALL %d ANSWER", id);
						SkypeAPI_SendCommand(buf);
					} else if (skype_config.AnswerMode == SAM_ANSWERING_SYSTEM) {
						SKYPE_CALL * call = GetCallHandle(id);
						if (!call) {
							CreateCallHandle(id);
							sprintf(buf, "GET CALL %d PARTNER_HANDLE", id);
							SkypeAPI_SendCommand(buf);
							sprintf(buf, "GET CALL %d TYPE", id);
							SkypeAPI_SendCommand(buf);
						}
					}
				} else if (!stricmp(q.c_str(), "INPROGRESS")) {
					SKYPE_CALL * call = GetCallHandle(id);
					if (call) {
						call->onHold = false;
					}
				} else if (!stricmp(q.c_str(), "ONHOLD") || !stricmp(q.c_str(), "LOCALHOLD")) {
					SKYPE_CALL * call = GetCallHandle(id);
					if (call) {
						call->onHold = true;
					}
				} else if (!stricmp(q.c_str(), "REFUSED")) {
					SKYPE_CALL * call = GetCallHandle(id);
					if (call) { CloseCallHandle(call); }
				} else if (!stricmp(q.c_str(), "FINISHED")) {
					SKYPE_CALL * call = GetCallHandle(id);
					if (call) { CloseCallHandle(call); }
				}
			}

			if (!stricmp(p.c_str(), "PARTNER_HANDLE")) {
				SKYPE_CALL * call = GetCallHandle(id);
				if (call) {
					sstrcpy(call->nick, st.stdGetSingleTok(4).c_str());
					snprintf(call->hostmask, sizeof(call->hostmask), "%s!%s@skype.com", call->nick, call->nick);
					call->uflags = api->user->GetUserFlags(call->hostmask);
					if ((call->uflags & UFLAG_SKYPE) && call->isIncoming) {
						api->ib_printf(_("Skype -> Attempting to answer call with Answering System...\n"));
						AcceptCall(call);
					}
				}
			}

			if (!stricmp(p.c_str(), "TYPE")) {
				SKYPE_CALL * call = GetCallHandle(id);
				if (call) {
					if (!strnicmp(st.stdGetSingleTok(4).c_str(), "INCOMING_", 9)) {
						call->isIncoming = true;
					}
					if (strlen(call->nick) && (call->uflags & UFLAG_SKYPE) && call->isIncoming) {
						api->ib_printf(_("Skype -> Attempting to answer call with Answering System...\n"));
						AcceptCall(call);
					}
				}
			}

			if (!stricmp(p.c_str(), "CAN_TRANSFER")) {
				SKYPE_CALL * call = GetCallHandle(id);

				p = st.stdGetSingleTok(5);
				if (!stricmp(p.c_str(), "TRUE")) {
					api->ib_printf(_("Skype -> Transferring incoming call to %s\n"), st.stdGetSingleTok(4).c_str());
					if (call) {
						CloseCallHandle(call);
					}
					sprintf(buf, "ALTER CALL %d TRANSFER %s", id, st.stdGetSingleTok(4).c_str());
					SkypeAPI_SendCommand(buf);
				} else {
					if (call) {
						strcpy(call->TextToSpeak, _("This call cannot be transferred to that person or phone number"));
					} else {
						api->ib_printf(_("Skype -> Cannot transfer incoming call to %s, hanging up...\n"), st.stdGetSingleTok(4).c_str());
						sprintf(buf, "ALTER CALL %d HANGUP | REDIRECT_TO_VOICEMAIL | FORWARD_CALL", id);
						SkypeAPI_SendCommand(buf);
					}
				}
			}

			if (!stricmp(p.c_str(), "DTMF")) {
				SKYPE_CALL * call = GetCallHandle(id);
				if (call) {
					p = st.stdGetSingleTok(4);
					SetCallDTMF(call, p.c_str()[0]);
				}
			}
		}
	}

	RelMutex(hMutex);
	return true;
}

SKYPE_API_CLIENT cli = {
#if defined(WIN32)
	NULL,
#else
	"RadioBot_Skype_Plugin",
#endif
	SKYPE_PROTOCOL_7,
	status_cb,
	message_cb
};


THREADTYPE SkypeThread(void * lpData) {
	TT_THREAD_START
	skype_config.connected = SkypeAPI_Connect(&cli);
	skype_config.lastRecv = time(NULL);
	time_t nextTry = time(NULL)+30;
	while (!skype_config.shutdown_now) {
		LockMutex(hMutex);
		if (skype_config.reconnect) {
			SkypeAPI_Disconnect();
			skype_config.connected = false;
			skype_config.reconnect = false;
			nextTry = time(NULL)+30;
		}
		if (!skype_config.connected && time(NULL) >= nextTry) {
			skype_config.connected = SkypeAPI_Connect(&cli);
			skype_config.lastRecv = time(NULL);
			nextTry = time(NULL)+30;
		}
		if (skype_config.connected) {
			if (time(NULL) > (skype_config.lastRecv+60)) {
				api->ib_printf(_("Skype -> API Timeout, attempting reconnect...\n"));
				skype_config.reconnect = true;
			} else if (time(NULL) > (skype_config.lastRecv+30)) {
				SkypeAPI_SendCommand("PING");
			}
		}
		for (msgListType::iterator x = msgList.begin(); x != msgList.end(); x++) {
			if (x->second.ts < (time(NULL)-30)) {
				msgList.erase(x);
				//x = msgList.begin();
				break;
			}
		}
		RelMutex(hMutex);
		safe_sleep(1);
	}
	if (skype_config.connected) {
		skype_config.reconnect = skype_config.connected = false;
		SkypeAPI_Disconnect();
	}
	TT_THREAD_END
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;
	if (api->ss == NULL) {
		api->ib_printf2(pluginnum,_("Skype -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	int err = mpg123_init();
	if ( err != MPG123_OK) {
		api->ib_printf("Skype -> Error initializing mp3 decoder: %s\n", mpg123_plain_strerror(err));
		mpg123_exit();
		return 0;
	}

	memset(&skype_config, 0, sizeof(skype_config));

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Skype");
	if (sec) {
		skype_config.RequirePrefix = api->config->GetConfigSectionLong(sec, "RequirePrefix") > 0 ? true:false;
		char * p = api->config->GetConfigSectionValue(sec, "AnswerMode");
		if (p) {
			if (!stricmp(p, "AutoAnswer")) {
				skype_config.AnswerMode = SAM_AUTO_ANSWER;
			} else if (!stricmp(p, "Transfer")) {
				skype_config.AnswerMode = SAM_TRANSFER;
			} else if (!stricmp(p, "AnsweringSystem")) {
				skype_config.AnswerMode = SAM_ANSWERING_SYSTEM;
			} else {
				api->ib_printf(_("Skype -> Error: Unknown AnswerMode: %s\n"), p);
			}
		}

		api->config->GetConfigSectionValueBuf(sec, "TransferCallsTo", skype_config.TransferCallsTo, sizeof(skype_config.TransferCallsTo));
		api->config->GetConfigSectionValueBuf(sec, "MoodTextString", skype_config.MoodTextString, sizeof(skype_config.MoodTextString));
	}

	if (api->SendMessage(-1, TTS_GET_INTERFACE, (char *)&skype_config.tts, sizeof(&skype_config.tts)) != 1 && skype_config.AnswerMode == SAM_ANSWERING_SYSTEM) {
		api->ib_printf(_("Skype -> Error: TTS Services plugin is not loaded! You must load the TTS Services plugin in order to use the Skype plugin Answering System.\n"));
		skype_config.AnswerMode = SAM_DO_NOTHING;
	}

	TT_StartThread(SkypeThread, NULL, _("Skype Thread"), pluginnum);

	return 1;
}

void quit() {
	if (CallCount()) {
		HangUpAllCalls();
		safe_sleep(3);
		int tries;
		tries=0;
		while (CallCount()) {
			safe_sleep(100,true);
			tries++;
			if (tries == 10) {
				CloseAllCalls();
				tries = 0;
			}
		}
	}
	skype_config.shutdown_now = true;
	api->ib_printf(_("Skype -> Shutting down...\n"));
	while (TT_NumThreadsWithID(pluginnum)) {
		safe_sleep(100,true);
	}
	mpg123_exit();
	api->ib_printf(_("Skype -> Shut down.\n"));
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
#if !defined(IRCBOT_LITE)
		case IB_SS_DRAGCOMPLETE:{
				if (skype_config.connected && *((bool *)data) == true && skype_config.MoodTextString[0] != 0) {
					char buf[1024];
					snprintf(buf, sizeof(buf), "SET PROFILE RICH_MOOD_TEXT %s", skype_config.MoodTextString);
					api->ProcText(buf, sizeof(buf));
					LockMutex(hMutex);
					SkypeAPI_SendCommand(buf);
					RelMutex(hMutex);
				}
			}
			break;
#endif
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{AA4767B9-54AC-4edd-A58B-2B4219C88025}",
	"Skype",
	"Skype Plugin 1.0",
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
