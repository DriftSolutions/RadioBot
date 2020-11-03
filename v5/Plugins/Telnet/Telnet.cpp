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
#include <algorithm>

Titus_Sockets3 * sockets = NULL;
BOTAPI_DEF *api = NULL;
int pluginnum; // the number we were assigned

TELNET_CONFIG telconf;
Titus_Mutex chatMutex;
chatListType sessions;

int Telnet_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

void LoadConfig() {
	memset(&telconf,0,sizeof(telconf));
	telconf.Port  = 10002;
	telconf.MaxSessions = 4;
	sstrcpy(telconf.SSL_Cert, "ircbot.pem");
	DS_CONFIG_SECTION * Scan = api->config->GetConfigSection(NULL, "Telnet");
	if (Scan) {
		long lVal = api->config->GetConfigSectionLong(Scan, "Port");
		if (lVal > 0) {
			telconf.Port = lVal;
		}
		lVal = api->config->GetConfigSectionLong(Scan, "MaxSessions");
		if (lVal > 0) {
			telconf.MaxSessions = lVal;
		}
		char * val = api->config->GetConfigSectionValue(Scan,"SSL_Cert");
		if (val) {
			sstrcpy(telconf.SSL_Cert,val);
		}
	} else {
		api->ib_printf(_("Telnet -> No 'Telnet' block found in config, using pure defaults...\n"));
	}
}

int init(int num, BOTAPI_DEF * BotAPI) {
	char buf[256];
	pluginnum=num;
	api=BotAPI;
	LoadConfig();

	sockets = new Titus_Sockets();
	telconf.lSock = sockets->Create();
	sockets->SetReuseAddr(telconf.lSock, true);
	memset(buf, 0, sizeof(buf));
	bool bound = (api->GetBindIP(buf, sizeof(buf)) != NULL) ? sockets->BindToAddr(telconf.lSock, buf, telconf.Port):sockets->Bind(telconf.lSock, telconf.Port);
	if (!bound) {
		api->ib_printf(_("Telnet -> Error binding port %d: %s\n"), telconf.Port, sockets->GetLastErrorString());
		delete sockets;
		return 0;
	}
	if (!sockets->Listen(telconf.lSock)) {
		api->ib_printf(_("Telnet -> Error listen()ing port %d: %s\n"), telconf.Port, sockets->GetLastErrorString());
		delete sockets;
		return 0;
	}

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "telnet-status", &acl, CM_ALLOW_ALL_PRIVATE, Telnet_Commands, "Shows you any active Telnet sessions");

	TT_StartThread(ChatThread, NULL, _("Telnet Listen Thread"), pluginnum);
	api->ib_printf(_("Telnet -> Waiting for connections on port %d...\n"), telconf.Port);
	return 1;
}

void quit() {
	api->ib_printf(_("Telnet -> Shutting down...\n"));
	telconf.shutdown_now=true;
	//main listening socket will be closed by ChatThread when it exits
	time_t timeo = time(NULL) + 30;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf(_("Telnet -> Waiting on (%d) threads to die...\n"), TT_NumThreadsWithID(pluginnum));
		api->safe_sleep_seconds(1);
	}
	delete sockets;
	sockets = NULL;
	api->ib_printf(_("Telnet -> Shut down.\n"));
}


int Telnet_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024],buf2[1024],buf3[256];

	if (!stricmp(command, "telnet-status")) {
		ut->std_reply(ut, _("Telnet Status"));
		chatMutex.Lock();
		sprintf(buf, _("Active Telnet Sessions: %d"), sessions.size());
		ut->std_reply(ut, buf);
		for (chatListType::const_iterator x = sessions.begin(); x != sessions.end(); x++) {
			if ((*x)->status == DS_AUTHENTICATED) {
				api->user->uflag_to_string((*x)->User->Flags, buf3, sizeof(buf3));
				snprintf(buf, sizeof(buf), _(" %s[%s]: %s:%d, idle: %s"), (*x)->User->Nick, buf3, (*x)->sock->remote_ip, (*x)->sock->remote_port, api->textfunc->duration(time(NULL) - (*x)->timestamp, buf2));
			} else {
				snprintf(buf, sizeof(buf), _(" logging in, %s:%d, idle: %s"), (*x)->sock->remote_ip, (*x)->sock->remote_port, api->textfunc->duration(time(NULL) - (*x)->timestamp, buf2));
			}
			ut->std_reply(ut, buf);
		}
		chatMutex.Release();
		return 1;
	}

	return 0;
}

int message_proc(unsigned int msg, char * data, int datalen) {
	if (msg == IB_LOG) {
		chatMutex.Lock();
		for (chatListType::iterator x = sessions.begin(); x != sessions.end(); x++) {
			if ((*x)->status == DS_AUTHENTICATED && (*x)->console && api->user->uflag_have_any_of((*x)->User->Flags, UFLAG_MASTER)) {
				time_t tme=0;
				time(&tme);
				tm tml;
				localtime_r(&tme, &tml);
				char ib_time[64];
				strftime(ib_time, sizeof(ib_time), _("<%m/%d/%y@%I:%M:%S%p>"), &tml);

				/*
				int n = strlen(ib_time);
				memmove(buf+n+1, buf, strlen(buf)+1);
				strcpy(buf, ib_time);
				buf[strlen(ib_time)]=' ';
				fprintf(config.base.log_fp, "%s", buf)
				*/
				std::string buf = ib_time;
				buf += " ";
				buf += data;
				//buf = std::replace(buf.begin(), buf.end(), "\n", "\r\n");
				//buf += "\r\n";
				chatMutex.Lock();
				sockets->Send((*x)->sock, (char *)buf.c_str());
				chatMutex.Release();
			}
		}
		chatMutex.Release();
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{AD84E9A9-2A90-471D-8671-DCE34D15E317}",
	"Telnet Support",
	"Telnet Support Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message_proc,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL

