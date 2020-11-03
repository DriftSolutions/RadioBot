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

int pluginnum=0;
BOTAPI_DEF * api=NULL;

SMS_CONFIG sms_config;

bool LoadConfig() {
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "SMS");
	if (sec == NULL) {
		api->ib_printf(_("SMS Services -> No 'SMS' section found in your config, aborting...\n"));
		return false;
	}

	DS_CONFIG_SECTION * s = api->config->GetConfigSection(sec, "POP3");
	if (s) {
		if (!api->config->GetConfigSectionValueBuf(s, "User", sms_config.pop3.user, sizeof(sms_config.pop3.user))) {
			api->ib_printf(_("SMS Services -> No 'User' in your 'SMS/POP3' section, aborting...\n"));
			return false;
		}
		if (!api->config->GetConfigSectionValueBuf(s, "Pass", sms_config.pop3.pass, sizeof(sms_config.pop3.pass))) {
			api->ib_printf(_("SMS Services -> No 'Pass' in your 'SMS/POP3' section, aborting...\n"));
			return false;
		}
		if (!api->config->GetConfigSectionValueBuf(s, "Host", sms_config.pop3.host, sizeof(sms_config.pop3.host))) {
			api->ib_printf(_("SMS Services -> No 'Host' in your 'SMS/POP3' section, aborting...\n"));
			return false;
		}
		sms_config.pop3.port = 110;
		if (api->config->GetConfigSectionLong(s, "Port") > 0) {
			sms_config.pop3.port = api->config->GetConfigSectionLong(s, "Port");
		}
		int n = api->config->GetConfigSectionLong(s, "SSL");
		if (n == 1 || n == 0) {
			sms_config.pop3.ssl = n;
		} else {
			sms_config.pop3.ssl = -1;
		}
	} else {
		api->ib_printf(_("SMS Services -> No 'SMS/POP3' section found in your config, aborting...\n"));
		return false;
	}

	s = api->config->GetConfigSection(sec, "GoogleVoice");
	if (s) {
		if (!api->config->GetConfigSectionValueBuf(s, "User", sms_config.voice.user, sizeof(sms_config.voice.user))) {
			api->ib_printf(_("SMS Services -> No 'User' in your 'SMS/GoogleVoice' section, aborting...\n"));
			return false;
		}
		if (!api->config->GetConfigSectionValueBuf(s, "Pass", sms_config.voice.pass, sizeof(sms_config.voice.pass))) {
			api->ib_printf(_("SMS Services -> No 'Pass' in your 'SMS/GoogleVoice' section, aborting...\n"));
			return false;
		}
	} else {
		api->ib_printf(_("SMS Services -> No 'SMS/GoogleVoice' section found in your config, aborting...\n"));
		return false;
	}

	sms_config.RequirePrefix = (api->config->GetConfigSectionLong(sec, "RequirePrefix") > 0) ? true:false;
	sms_config.LineMerge = (api->config->GetConfigSectionLong(sec, "LineMerge") > 0) ? true:false;
	sms_config.HostmaskForm = api->config->GetConfigSectionLong(sec, "HostmaskForm");
	if (sms_config.HostmaskForm < 0) { sms_config.HostmaskForm = 0; }
	return true;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum = num;
	api = BotAPI;
	memset(&sms_config,0,sizeof(sms_config));
	if (LoadConfig()) {
		#if !defined(WIN32)
		signal(SIGSEGV,SIG_IGN);
		signal(SIGPIPE,SIG_IGN);
		#endif
		sms_config.enabled = true;
		TT_StartThread(POP_Thread, NULL, "POP3 Thread", pluginnum);
		TT_StartThread(Send_Thread, NULL, "SMS Send Thread", pluginnum);
	}
	return 1;
}

void quit() {
	api->ib_printf(_("SMS Services -> Beginning shutdown...\n"));

	//api->commands->UnregisterCommandByName( "relay" );

	sms_config.shutdown_now = true;
	int64 timeo = time(NULL) + 15;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf(_("SMS Services -> Waiting on threads to die (%d) (Will wait " I64FMT " more seconds...)\n"),TT_NumThreads(),timeo - time(NULL));
		TT_PrintRunningThreadsWithID(pluginnum);
		api->safe_sleep_seconds(1);
	}

	api->ib_printf(_("SMS Services -> Plugin Unloaded!\n"));
}

void send_sms(const char * phone, const char * text) {
	Send_SMS(phone, text);
}

SMS_INTERFACE sms_interface = {
	send_sms,
};


int MessageHandler(unsigned int msg, char * data, int datalen) {
	//printf("MessageHandler(%u, %X, %d)\n", msg, data, datalen);

	switch(msg) {
		case SMS_GET_INTERFACE:{
				memcpy(data, &sms_interface, sizeof(sms_interface));
				return 1;
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{53AEA170-4BCC-4308-9ADB-AD52380ECE63}",
	"SMS",
	"SMS Plugin",
	"Drift Solutions",
	init,
	quit,
	MessageHandler,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
