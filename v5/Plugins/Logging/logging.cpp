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

#include "logging.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned
LOG_CONFIG log_config;

Titus_Mutex logMutex;
typedef std::vector<mIRC_Log *> LogsType;
LogsType logs;

mIRC_Log * FindLog(int netno, std::string chan) {
	logMutex.Lock();
	for (LogsType::const_iterator x = logs.begin(); x != logs.end(); x++) {
		if ((*x)->netno == netno && !stricmp((*x)->channel.c_str(), chan.c_str())) {
			logMutex.Release();
			return *x;
		}
	}

	mIRC_Log * ret = new mIRC_Log(netno, chan);
	logs.push_back(ret);
	logMutex.Release();
	return ret;
}

void CloseAllLogs() {
	logMutex.Lock();
	for (LogsType::const_iterator x = logs.begin(); x != logs.end(); x++) {
		delete *x;
	}
	logs.clear();
	logMutex.Release();
}

/*
int Logging_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];

	if (!stricmp(command, "rehash") && (parms || strlen(parms))) {
		ut->std_reply(ut, "Error: You can only use !rehash without a filename in Demo Mode.");
		return 1;
	}

	return 0;
}
*/

void LoadConfig() {
	sstrcpy(log_config.log_dir, "." PATH_SEPS "logs" PATH_SEPS "mirc_logging");
	if (access("." PATH_SEPS "logs", 0) != 0) {
		dsl_mkdir("." PATH_SEPS "logs", 0700);
	}
	if (access(log_config.log_dir, 0) != 0) {
		dsl_mkdir(log_config.log_dir, 0700);
	}
	sstrcpy(log_config.log_filemask, "%netno%.%chan%.%date%.log");
	sstrcpy(log_config.log_datemask, "%m.%d.%Y");

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Logging");
	if (sec) {
		const char * p = api->config->GetConfigSectionValue(sec, "FileMask");
		if (p) {
			strlcpy(log_config.log_filemask, p, sizeof(log_config.log_filemask));
		}
		p = api->config->GetConfigSectionValue(sec, "DateMask");
		if (p) {
			strlcpy(log_config.log_datemask, p, sizeof(log_config.log_datemask));
		}
	}
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Logging Plugin -> This version of RadioBot is not supported!\n"));
		return 0;
	}
	memset(&log_config, 0, sizeof(log_config));
	LoadConfig();
	return 1;
}

void quit() {
	api->ib_printf(_("Logging Plugin -> Shutting down...\n"));
	CloseAllLogs();
}

int message(unsigned int MsgID, char * data, int datalen) {
	char buf[4096];
	switch(MsgID) {
		case IB_ONJOIN:{
				USER_PRESENCE * oj = (USER_PRESENCE *)data;
				if (oj == NULL) { return 0; }
				if (!stricmp(oj->Nick, api->irc->GetCurrentNick(oj->NetNo))) {
					sprintf(buf, "*** Now talking in %s\r\n", oj->Channel);
					FindLog(oj->NetNo, oj->Channel)->WriteLine(buf);
				} else {
					const char *p = strchr(oj->Hostmask,'!');
					if (p) {
						p++;
					} else {
						p = oj->Hostmask;
					}
					sprintf(buf, "*** %s (%s) has joined %s\r\n", oj->Nick, p, oj->Channel);
					FindLog(oj->NetNo, oj->Channel)->WriteLine(buf);
				}
			}
			break;

		case IB_ONQUIT:{
				IBM_USERTEXT * oj = (IBM_USERTEXT *)data;
				if (oj == NULL || oj->userpres->Channel == NULL) { return 0; }
				sprintf(buf, "*** %s has quit IRC (%s)\r\n", oj->userpres->Nick, oj->text);
				FindLog(oj->userpres->NetNo, oj->userpres->Channel)->WriteLine(buf);
			}
			break;

		case IB_ONNICK:{
				IBM_NICKCHANGE * oj = (IBM_NICKCHANGE *)data;
				if (oj == NULL || oj->channel == NULL) { return 0; }
				sprintf(buf, "*** %s is now known as %s\r\n", oj->old_nick, oj->new_nick);
				FindLog(oj->netno, oj->channel)->WriteLine(buf);
			}
			break;

		case IB_ONKICK:{
				IBM_ON_KICK * oj = (IBM_ON_KICK *)data;
				if (oj == NULL || oj->kicker->Channel == NULL) { return 0; }
				if (!stricmp(oj->kickee, api->irc->GetCurrentNick(oj->kicker->NetNo))) {
					sprintf(buf, "*** You were kicked by %s (%s)\r\n", oj->kicker->Nick, oj->reason);
				} else {
					sprintf(buf, "*** %s was kicked by %s (%s)\r\n", oj->kickee, oj->kicker->Nick, oj->reason);
				}
				FindLog(oj->kicker->NetNo, oj->kicker->Channel)->WriteLine(buf);
			}
			break;

		case IB_ONPART:{
				IBM_USERTEXT * oj = (IBM_USERTEXT *)data;
				if (oj == NULL || oj->userpres->Channel == NULL) { return 0; }
				const char *p = strchr(oj->userpres->Hostmask,'!');
				if (p) {
					p++;
				} else {
					p = oj->userpres->Hostmask;
				}
				sprintf(buf, "*** %s (%s) has left %s (%s)\r\n", oj->userpres->Nick, p, oj->userpres->Channel, oj->text);
				FindLog(oj->userpres->NetNo, oj->userpres->Channel)->WriteLine(buf);
			}
			break;

		case IB_ONTOPIC:{
				IBM_USERTEXT * oj = (IBM_USERTEXT *)data;
				if (oj == NULL || oj->userpres->Channel == NULL) { return 0; }
				sprintf(buf, "*** %s changes topic to '%s'\r\n", oj->userpres->Nick, oj->text);
				FindLog(oj->userpres->NetNo, oj->userpres->Channel)->WriteLine(buf);
			}
			break;

		case IB_ONTEXT:{
				IBM_USERTEXT * oj = (IBM_USERTEXT *)data;
				if (oj == NULL || oj->type != IBM_UTT_CHANNEL || oj->userpres->Channel == NULL) { return 0; }
				sprintf(buf, "<%s> %s\r\n", oj->userpres->Nick, oj->text);
				FindLog(oj->userpres->NetNo, oj->userpres->Channel)->WriteLine(buf);
			}
			break;

		case IB_ONNOTICE:{
				IBM_USERTEXT * oj = (IBM_USERTEXT *)data;
				if (oj == NULL || oj->type != IBM_UTT_CHANNEL || oj->userpres->Channel == NULL) { return 0; }
				sprintf(buf, "-%s- %s\r\n", oj->userpres->Nick, oj->text);
				FindLog(oj->userpres->NetNo, oj->userpres->Channel)->WriteLine(buf);
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{757F5E37-C075-4928-919F-A3C1DE0BBCAB}",
	"Demo Mode",
	"Demo Mode Plugin 1.0",
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
