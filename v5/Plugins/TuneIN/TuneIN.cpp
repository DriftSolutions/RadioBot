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

#include "TuneIN.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

char last_title[520]={0};
time_t startTime=0;
int songLen = 0;
IBM_SONGINFO si={0};

TUNEIN_CONFIG tunein_config;

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	api->ib_printf(_("TuneIN Support -> Loading...\n"));

	if (api->ss == NULL) {
		api->ib_printf2(pluginnum,_("TuneIN Support -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	memset(&tunein_config, 0, sizeof(tunein_config));

	DS_CONFIG_SECTION * sec;
	sec = api->config->GetConfigSection(NULL, "TuneIN");
	if (sec) {
		api->config->GetConfigSectionValueBuf(sec, "PartnerID", tunein_config.part_id, sizeof(tunein_config.part_id));
		api->config->GetConfigSectionValueBuf(sec, "PartnerKey", tunein_config.part_key, sizeof(tunein_config.part_key));
		api->config->GetConfigSectionValueBuf(sec, "StationID", tunein_config.station_id, sizeof(tunein_config.station_id));
		if (strlen(tunein_config.part_id) && strlen(tunein_config.part_key) && strlen(tunein_config.station_id)) {
			tunein_config.enabled = true;
		}
		tunein_config.source_only = api->config->GetConfigSectionLong(sec, "SourceOnly") > 0 ? true:false;
	}

	api->ib_printf(_("TuneIN Support -> %s!\n"), tunein_config.enabled?"Loaded succesfully":"Disabled, no PartnerID, PartnerKey, and/or StationID specified in config");
	return tunein_config.enabled;
}

void quit() {
	api->ib_printf(_("TuneIN Support -> Shutting down...\n"));
	tunein_config.enabled = false;
	api->ib_printf(_("TuneIN Support -> Shut down.\n"));
}

size_t curl_write(void *ptr, size_t size, size_t nmemb, void *stream) {
	return fwrite(ptr, size, nmemb, (FILE *)stream);
}


bool tunein_sendTitle(const char * artist, const char * album, const char * title) {
	if (title == NULL || title[0] == 0) {
		api->ib_printf("TuneIN Support -> No title specified (%s/%s/%s)\n", artist, album, title);
		return false;
	}

	api->ib_printf("TuneIN Support -> Sending song information...\n");

	bool ret = false;
	char buf[1024];//,buf2[128];
	memset(buf, 0, sizeof(buf));

	CURL * h = api->curl->easy_init();
	if (h != NULL) {
		FILE * fp = fopen("./tmp/tunein.tmp", "wb");
		if (fp != NULL) {
			char errbuf[CURL_ERROR_SIZE+1];
			memset(errbuf, 0, sizeof(errbuf));
			stringstream url;
			//http://air.radiotime.com/Playing.ashx?partnerId=<id>&partnerKey=<key>&id=<stationid>&title=Bad+Romance&artist=Lady+Gaga
			url << "http://air.radiotime.com/Playing.ashx?partnerId=";
			char * tmp = api->curl->escape(tunein_config.part_id, strlen(tunein_config.part_id));
			url << tmp;
			api->curl->free(tmp);
			url << "&partnerKey=";
			tmp = api->curl->escape(tunein_config.part_key, strlen(tunein_config.part_key));
			url << tmp;
			api->curl->free(tmp);
			url << "&id=";
			tmp = api->curl->escape(tunein_config.station_id, strlen(tunein_config.station_id));
			url << tmp;
			url << "&title=";
			tmp = api->curl->escape(title, strlen(title));
			url << tmp;
			api->curl->free(tmp);
			if (artist && artist[0]) {
				url << "&artist=";
				tmp = api->curl->escape(artist, strlen(artist));
				url << tmp;
				api->curl->free(tmp);
			}
			if (album && album[0]) {
				url << "&album=";
				tmp = api->curl->escape(album, strlen(album));
				url << tmp;
				api->curl->free(tmp);
			}
			api->curl->easy_setopt(h, CURLOPT_URL, url.str().c_str());
			api->curl->easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);
			api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
			api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1);
			api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 60);
			api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10);
			api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, curl_write);
			api->curl->easy_setopt(h, CURLOPT_WRITEDATA, fp);
			//api->curl->easy_setopt(h, CURLOPT_POST, 1);

			if (api->curl->easy_perform(h) == CURLE_OK) {
				long http_code = 0;
				api->curl->easy_getinfo(h, CURLINFO_RESPONSE_CODE, &http_code);
				if (http_code == 200) {
					api->ib_printf("TuneIN Support -> Song submitted!\n");
					ret = true;
				} else {
					api->ib_printf("TuneIN Support -> Error submitting song: %u\n", http_code);
				}
			} else {
				api->ib_printf("TuneIN Support -> Error communicating with TuneIN server: %s!\n", errbuf);
			}
			fclose(fp);
			if (ret) {
				remove("./tmp/tunein.tmp");
			}
		} else {
			api->ib_printf("TuneIN Support -> Error opening temp file!\n");
		}
		api->curl->easy_cleanup(h);
	} else {
		api->ib_printf("TuneIN Support -> Error creating CURL handle!\n");
	}
	return ret;
}


int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
		case IB_SS_DRAGCOMPLETE:{
				bool * b = (bool *)data;
				STATS * s = api->ss->GetStreamInfo();
				if (tunein_config.enabled && *b == true && stricmp(last_title, s->cursong)) {
					sstrcpy(last_title, s->cursong);
					if (!tunein_config.source_only || api->SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0) == 1) {
						IBM_SONGINFO si;
						memset(&si,0,sizeof(si));
						if (api->SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0) == 1 && api->SendMessage(-1, SOURCE_GET_SONGINFO, (char *)&si, sizeof(si)) == 1) {
							tunein_sendTitle(si.artist, si.album, si.title);
						} else {
							char * sep = strchr(last_title, '-');
							if (sep) {
								char artist[520]={0};
								strncpy(artist, last_title, sep-last_title-1);
								strtrim(artist);
								strtrim(++sep);
								tunein_sendTitle(artist, NULL, sep);
							} else {
								tunein_sendTitle(NULL, NULL, last_title);
							}
						}
					}
				}
			}
			break;
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{15585313-EB7C-4866-8CD1-DC594FE0C94D}",
	"TuneIN Plugin",
	"TuneIN Plugin",
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
