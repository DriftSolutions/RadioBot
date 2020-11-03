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

#include "welcome.h"

#define WS_MAX_RESULTS 10
struct WEBSEARCHRESULTS {
	int num;
	char * titles[WS_MAX_RESULTS];
	char * urls[WS_MAX_RESULTS];
};
void FreeWebSearchResults(WEBSEARCHRESULTS * ws) {
	for (int i=0; i < ws->num; i++) {
		zfreenn(ws->titles[i]);
		zfreenn(ws->urls[i]);
	}
}

void DoWebSearch(WEB_QUEUE * Scan) {
	char buf[2048], errbuf[1024];

	char fn[1024];
	//md5c(Scan->data,strlen(Scan->data), errbuf);
	hashdata("md5", (uint8 *)Scan->data, strlen(Scan->data), errbuf, sizeof(errbuf));
	sprintf(fn, "./tmp/bing.%s.tmp", errbuf);

	stringstream url;
	char * tmp = api->curl->escape(Scan->data, strlen(Scan->data));
	url << "https://api.datamarket.azure.com/Bing/SearchWeb/Web?Query=%27" << tmp << "%27&$top=15&$format=Atom";
	api->curl->free(tmp);

	char userpwd[256];
	sstrcpy(userpwd, wel_config.BingAccountKey);
	strlcat(userpwd, ":", sizeof(userpwd));
	strlcat(userpwd, wel_config.BingAccountKey, sizeof(userpwd));
	CACHED_DOWNLOAD_OPTIONS opts;
	memset(&opts, 0, sizeof(opts));
	opts.userpwd = userpwd;
	opts.cache_time_limit = 3600;
	char * file = get_cached_download(url.str().c_str(), fn, errbuf, sizeof(errbuf), &opts);
	if (file) {
		TiXmlDocument doc;
		doc.Parse(file);
		if (!doc.Error()) {
			WEBSEARCHRESULTS ws;
			memset(&ws, 0, sizeof(ws));
			TiXmlElement * root = doc.FirstChildElement("feed");
			if (root) {
				TiXmlElement * e = root->FirstChildElement("entry");
				while (e && ws.num < WS_MAX_RESULTS) {
					TiXmlElement * content = e->FirstChildElement("content");
					if (content) {
						TiXmlElement * prop = content->FirstChildElement("m:properties");
						if (prop) {
							TiXmlElement * title = prop->FirstChildElement("d:Title");
							if (title) {
								ws.titles[ws.num] = zstrdup(title->GetText());
							}
							TiXmlElement * url = prop->FirstChildElement("d:Url");
							if (url) {
								ws.urls[ws.num] = zstrdup(url->GetText());
							}
							if (ws.titles[ws.num] && ws.urls[ws.num]) {
								ws.num++;
							}
						}
					}
					e = e->NextSiblingElement("entry");
				}
			}
			if (ws.num) {
				stringstream str2;
				str2 << "Search results for \"" << Scan->data << "\":";
				Scan->pres->std_reply(Scan->pres, str2.str().c_str());
				for (int i=0; i < ws.num; i++) {
					stringstream str;
					str << (i+1) << ". \"" << ws.titles[i] << "\" - " << ws.urls[i];
					Scan->pres->std_reply(Scan->pres, str.str().c_str());
				}
				Scan->pres->std_reply(Scan->pres, "End of results.");
				FreeWebSearchResults(&ws);
			} else {
				Scan->pres->std_reply(Scan->pres, "No results found! (or messed up XML)");
			}
		} else {
			snprintf(buf, sizeof(buf), "Error parsing XML response: %s at %d:%d!", doc.ErrorDesc(), doc.ErrorRow(), doc.ErrorCol());
			Scan->pres->std_reply(Scan->pres, buf);
		}
		zfree(file);
	} else {
		snprintf(buf, sizeof(buf), "Error performing web search: %s!", errbuf);
		Scan->pres->std_reply(Scan->pres, buf);
	}
}
