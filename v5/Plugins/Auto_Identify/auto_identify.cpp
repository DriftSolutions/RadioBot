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

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

char nickserv[128];
char comp[256];
char pass[128];

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=(BOTAPI_DEF *)BotAPI;

	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Auto Identify -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "AutoIdentify");
	strcpy(nickserv,"NickServ");
	memset(&pass,0,sizeof(pass));
	if (sec) {
		char * val = api->config->GetConfigSectionValue(sec,"NickServ");
		if (val) {
			sstrcpy(nickserv, val);
		}
		api->config->GetConfigSectionValueBuf(sec,"Pass", pass, sizeof(pass));
		if (!strlen(pass)) {
			api->ib_printf(_("Auto Identify -> No 'Pass' Defined, disabling plugin!\n"));
			return 0;
		}
	} else {
		api->ib_printf(_("Auto Identify -> No AutoIdentify Section, disabling plugin!\n"));
		return 0;
	}
	sprintf(comp, "*msg*%s*IDENTIFY*", nickserv);
	return 1;
}

void quit() {
}

int message(unsigned int MsgID, char * data, int datalen) {
	if (MsgID == IB_ONNOTICE || MsgID == IB_ONTEXT) {
		IBM_USERTEXT * ut = (IBM_USERTEXT *)data;
		if (ut->type == IBM_UTT_PRIVMSG) {
			if (pass[0] && !stricmp(nickserv, ut->userpres->Nick) && (wildicmp(comp,ut->text) || wildicmp("*registered*nick*",ut->text))) {
				char buf[256];
				api->ib_printf(_("Auto Identify -> Identifying on Network %d\n"),ut->userpres->NetNo);
				sprintf(buf,"PRIVMSG %s :IDENTIFY %s\r\n", nickserv, pass);
				api->irc->SendIRC(ut->userpres->NetNo, buf, strlen(buf));
			}
		}
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{F70E987A-5284-436f-97A5-5E2FF133657F}",
	"Auto Identify",
	"Auto Identify Plugin 1.0",
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
