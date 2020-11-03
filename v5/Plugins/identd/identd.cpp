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

#include "../../src/plugins.h"

Titus_Sockets3 socks;

THREADTYPE ServerThread(VOID *lpData);

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned
int nothreads;
bool shutdown_now =false;

struct ID_CONFIG {
	char name[64];
	char platform[64];
	T_SOCKET * sock;
};

ID_CONFIG id_config;

void LoadConfig() {
	sstrcpy(id_config.name, api->irc->GetDefaultNick(-1));
	sstrcpy(id_config.platform, PLATFORM);

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "identd");
	if (!sec) { return; }

	char *p=NULL;
	if ((p = api->config->GetConfigSectionValue(sec,"Name")) != NULL) {
		sstrcpy(id_config.name,p);
	}
	if ((p = api->config->GetConfigSectionValue(sec,"Platform")) != NULL) {
		sstrcpy(id_config.platform,p);
	}
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	api->ib_printf(_("Identd Server -> Loading...\n"));
	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Identd Server -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	memset(&id_config, 0, sizeof(id_config));
	LoadConfig();

	TT_StartThread(ServerThread, NULL, _("Server Thread"), pluginnum);
	return 1;
}

void quit() {
	api->ib_printf(_("Identd Server -> Shutting down...\n"));
	shutdown_now=true;
	time_t timeo = time(NULL) + 30;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) <= timeo) {
		api->ib_printf(_("Identd Server -> Waiting on threads to die...\n"));
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}
	api->ib_printf(_("Identd Server -> Shut down.\n"));
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{635EF23F-B95E-4c72-B53F-72FFCEB3090E}",
	"Identd Server",
	"Identd Server Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	NULL,
	NULL,
	NULL,
	NULL
};

PLUGIN_EXPORT_FULL

THREADTYPE ClientThread(VOID *lpData) {
	TT_THREAD_START
	TITUS_SOCKET * sock = (TITUS_SOCKET *)tt->parm;

	char buf[1024],buf2[1024];
	int n=0;
	memset(&buf,0,sizeof(buf));

	while ((n = socks.RecvLine(sock,buf,sizeof(buf)-1)) > RL3_CLOSED && !shutdown_now) {
		if (n == 0 || n == RL3_NOLINE) {
			safe_sleep(250,true);
			continue;
		}

		buf[n]=0;

		sprintf(buf2,"%s : USERID : %s : %s\r\n",buf,id_config.platform,id_config.name);
		socks.Send(sock,buf2,strlen(buf2));
		break;
	}

	socks.Close(sock);
	TT_THREAD_END
}

THREADTYPE ServerThread(VOID *lpData) {
	TT_THREAD_START

	while (!shutdown_now) {

		api->ib_printf(_("Identd Server -> Attempting to start Identd Server...\n"));
		id_config.sock = socks.Create();

		if (id_config.sock) {

			socks.SetReuseAddr(id_config.sock, true);

			if (socks.Bind(id_config.sock, 113)) {
				if (socks.Listen(id_config.sock)) {

					api->ib_printf(_("Identd Server -> Identd Server Started Successfully!\n"));

					sockaddr_in addr;
					socklen_t addrlen = sizeof(addr);
					TITUS_SOCKET * rSock;
					timeval timeo;

					while (!shutdown_now) {
						timeo.tv_sec  = 1;
						timeo.tv_usec = 0;
						if (socks.Select_Read(id_config.sock, &timeo) > 0) {
							if ((rSock = socks.Accept(id_config.sock,(sockaddr *)&addr,&addrlen)) != NULL) {
								TT_StartThread(ClientThread, rSock, _("Identd Client Thread"), pluginnum);
							}
						}
					}
				} else {
					api->ib_printf(_("Identd Server -> Identd Server Failed to start! (listen)\n"));
				}
			} else {
				api->ib_printf(_("Identd Server -> Identd Server Failed to start! (bind)\n"));
			}

			socks.Close(id_config.sock);
		} else {
			api->ib_printf(_("Identd Server -> Identd Server Failed to start! (error creating socket)\n"));
		}

		if (!shutdown_now) {
			api->safe_sleep_seconds(5);
		}

	}

	TT_THREAD_END
}

