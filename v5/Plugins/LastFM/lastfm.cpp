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

//http://post.audioscrobbler.com/?hs=true&p=1.2.1&c=<client-id>&v=<client-ver>&u=<user>&t=<timestamp>&a=<auth>
#include "liblastfm.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

bool first_run = true;
char last_title[520]={0};
time_t startTime=0;
int songLen = 0;
IBM_SONGINFO si={0};

LFM_CONFIG lfm_config;

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	memset(&lfm_config, 0, sizeof(lfm_config));

	api->ib_printf(_("Last.FM Support -> Loading...\n"));
	if (api->ss == NULL) {
		api->ib_printf2(pluginnum,_("Last.FM Support -> This version of RadioBot is not supported!\n"));
		return 0;
	}
	memset(&lfm_config, 0, sizeof(lfm_config));

	DS_CONFIG_SECTION * sec;
	sec = api->config->GetConfigSection(NULL, "LastFM");
	if (sec) {
		char * p = api->config->GetConfigSectionValue(sec, "User");
		if (p) {
			sstrcpy(lfm_config.user, p);
		}
		p = api->config->GetConfigSectionValue(sec, "Pass");
		if (p) {
			sstrcpy(lfm_config.pass, p);
		}
		if (strlen(lfm_config.user) && strlen(lfm_config.pass)) {
			lfm_config.enabled = true;
		}
		lfm_config.source_only = api->config->GetConfigSectionLong(sec, "SourceOnly") > 0 ? true:false;
	}

	api->ib_printf(_("Last.FM Support -> %s!\n"), lfm_config.enabled?"Loaded succesfully":"Disabled, no user and/or password specified in config");
	return lfm_config.enabled;
}

void quit() {
	api->ib_printf(_("Last.FM Support -> Shutting down...\n"));
	lfm_config.enabled = false;
	api->ib_printf(_("Last.FM Support -> Shut down.\n"));
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
		case IB_SS_DRAGCOMPLETE:{
				bool * b = (bool *)data;
				if (lfm_config.enabled && *b == true) {
					if (!first_run) {
						if (songLen == 0) {
							songLen = time(NULL) - startTime;
						}
						if (songLen >= 240 || (songLen >= 90 && (time(NULL) - startTime) > (songLen/2))) {
							if (strlen(si.title)) {
								lfm_scrobble(si.artist, si.album, si.title, songLen, startTime);
							} else if (!lfm_config.source_only || api->SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0) == 1) {
								char * sep = strchr(last_title, '-');
								if (sep) {
									char artist[520]={0};
									strncpy(artist, last_title, sep-last_title-1);
									strtrim(artist);
									strtrim(++sep);
									lfm_scrobble(artist, NULL, sep, songLen, startTime);
								} else {
									lfm_scrobble(NULL, NULL, last_title, songLen, startTime);
								}
							}
						}
					}

					STATS * s = api->ss->GetStreamInfo();
					sstrcpy(last_title, s->cursong);
					if (api->SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0) == 1 && api->SendMessage(-1, SOURCE_GET_SONGINFO, (char *)&si, sizeof(si)) == 1) {
						songLen = si.songLen;
					} else {
						memset(&si, 0, sizeof(si));
						songLen = 0;
					}
					startTime = time(NULL);
					first_run = false;
				}
			}
			break;
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{75B419E9-8115-4fe8-BD0B-7C3CF73DD3B0}",
	"Last.FM Plugin",
	"Last.FM Plugin " VERSION "",
	"Drift Solutions",
	init,
	NULL,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
