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
#include "../../../Common/libremote/libremote.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned
RemoteClient * rClient = NULL;
Titus_Mutex hMutex;

typedef struct {
	char host[128];
	char user[128];
	char pass[128];
	int32 port;
	//bool usingshared;
} SHARED_CONFIG;

SHARED_CONFIG shared;

bool LoadConfig() {
	memset(&shared,0,sizeof(shared));
	shared.port = 10001; // set the default port

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Users_Shared");
	if (sec == NULL) { return false; }

	char * p = api->config->GetConfigSectionValue(sec,"Host");
	if (p) {
		sstrcpy(shared.host,p);
	}

	shared.port = api->config->GetConfigSectionLong(sec,"Port");
	if (shared.port <= 0 || shared.port > 65535) { shared.port = 10001; }

	p = api->config->GetConfigSectionValue(sec,"User");
	if (p) {
		sstrcpy(shared.user,p);
	}

	p = api->config->GetConfigSectionValue(sec,"Pass");
	if (p) {
		sstrcpy(shared.pass,p);
	}

	if (strlen(shared.host) && strlen(shared.user) && strlen(shared.pass)) {
		return true;
	}
	return false;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	api->ib_printf(_("Shared Users Plugin -> Loading...\n"));
	LockMutex(hMutex);
	if (!LoadConfig()) {
		RelMutex(hMutex);
		api->ib_printf(_("Shared Users Plugin -> Error loading, incomplete or no configuration found!\n"));
		return 0;
	}
	rClient = new RemoteClient();
	RelMutex(hMutex);
	api->ib_printf(_("Shared Users Plugin -> Loaded.\n"));
	return 1;
}

void quit() {
	api->ib_printf(_("Shared Users Plugin -> Shutting down...\n"));
	LockMutex(hMutex);
	memset(&shared, 0, sizeof(shared));
	delete rClient;
	RelMutex(hMutex);
	api->ib_printf(_("Shared Users Plugin -> Shutdown complete\n"));
}

bool auth_user(char * hostmask, IBM_USEREXTENDED * ue) {
	LockMutex(hMutex);
	if (rClient->IsReady()) {
		REMOTE_HEADER rHead;
		memset(&rHead, 0, sizeof(rHead));
		rHead.ccmd = RCMD_GETUSERINFO;
		rHead.datalen = uint32(strlen(hostmask));
		if (rClient->SendPacket(&rHead, hostmask)) {
			if (rClient->RecvPacket(&rHead, (char *)ue, sizeof(IBM_USEREXTENDED))) {
				if (rHead.scmd == RCMD_USERINFO) {
					api->ib_printf(_("OK\n"));
					RelMutex(hMutex);
					return true;
				} else if (rHead.scmd == RCMD_USERNOTFOUND) {
					api->ib_printf(_("User Not Found\n"));
				} else {
					api->ib_printf(_("Error communicating with main RadioBot, disconnecting...\n"));
					rClient->Disconnect();
				}
			} else {
				api->ib_printf(_("Error communicating with main RadioBot, disconnecting...\n"));
				rClient->Disconnect();
			}
		} else {
			api->ib_printf(_("Error communicating with main RadioBot, disconnecting...\n"));
			rClient->Disconnect();
		}
	} else {
		api->ib_printf(_("Not Connected\n"));
	}
	RelMutex(hMutex);
	return false;
}

int MsgProc(unsigned int msg, char * data, int datalen) {
	switch(msg) {
		case IB_GETUSERINFO:{
				IBM_GETUSERINFO * ui = (IBM_GETUSERINFO *)data;
				LockMutex(hMutex);
				if (!rClient->IsReady()) {
					static int64 lastAttempt=0;
					if ((time(NULL) - lastAttempt) > 30) {
						lastAttempt = time(NULL);
						api->ib_printf(_("Shared Users -> Connecting to main RadioBot...\n"));
						if (rClient->Connect(shared.host, shared.port, shared.user, shared.pass, false)) {
							api->ib_printf(_("Shared Users -> Connected Successfully!\n"));
						}
					}
				}
				RelMutex(hMutex);

				api->ib_printf(_("Shared Users -> Querying main RadioBot for %s (FETCH)... "), ui->ui.nick);
				if (auth_user(strlen(ui->ui.hostmask) ? ui->ui.hostmask:ui->ui.nick, &ui->ue)) {
					ui->type = IBM_UT_EXTENDED;
					return 1;
				}
				return 0;
			}
			break;

		case IB_DEL_USER:
		case IB_ADD_USER:
			break;
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{472A3621-771C-4862-B060-1C06F6F421D8}",
	"Users_Shared",
	"RadioBot Shared Users Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	MsgProc,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
