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

void DoWordnik(WEB_QUEUE * Scan) {
	char buf[2048], errbuf[1024];

	char fn[1024];
	hashdata("md5", (uint8 *)Scan->data, strlen(Scan->data), errbuf, sizeof(errbuf));
	sprintf(fn, "./tmp/wordnik.%s.tmp", errbuf);
	stringstream url;
	char * tmp = api->curl->escape(Scan->data, strlen(Scan->data));
	char * tmp2 = api->curl->escape(wel_config.RegKey, strlen(wel_config.RegKey));
	url << "https://www.shoutirc.com/wordnik.php?key=" << tmp2 << "&word=" << tmp;
	api->curl->free(tmp);
	api->curl->free(tmp2);

	CACHED_DOWNLOAD_OPTIONS opt;
	memset(&opt, 0, sizeof(opt));
	opt.cache_time_limit = 86400;
	char * file = get_cached_download(url.str().c_str(), fn, errbuf, sizeof(errbuf));
	if (file) {
		TiXmlDocument doc;
		doc.Parse(file);
		if (!doc.Error()) {
			TiXmlElement * root = doc.FirstChildElement("definitions");
			int count = root ? atoi(root->Attribute("count")):0;
			if (root && count > 0) {
				snprintf(buf, sizeof(buf), "Definitions for '%s' provided by Wordnik.com...", Scan->data);
				Scan->pres->std_reply(Scan->pres, buf);

				TiXmlElement * def = root->FirstChildElement("definition");
				while (def) {
					if (api->LoadMessage("Wordnik", buf, sizeof(buf))) {
						str_replace(buf, sizeof(buf), "%word%", def->GetText());
						str_replace(buf, sizeof(buf), "%type%", def->Attribute("type"));
					} else {
						if (!stricmp(Scan->pres->Desc,"IRC")) {
							snprintf(buf, sizeof(buf), "[\035%s\035] %s", def->Attribute("type"), def->GetText());
						} else {
							snprintf(buf, sizeof(buf), "[%s] %s", def->Attribute("type"), def->GetText());
						}
					}
					Scan->pres->std_reply(Scan->pres, buf);
					def = def->NextSiblingElement("definition");
				}

				Scan->pres->std_reply(Scan->pres, "End of list.");
			} else {
				root = doc.FirstChildElement("error");
				if (root) {
					snprintf(buf, sizeof(buf), "Error getting definitions word '%s': %s", Scan->data, root->GetText());
					Scan->pres->std_reply(Scan->pres, buf);
					remove(fn);
				} else {
					snprintf(buf, sizeof(buf), "No definitions found for word '%s'", Scan->data);
					Scan->pres->std_reply(Scan->pres, buf);
				}
			}
		} else {
			snprintf(buf, sizeof(buf), "Error parsing XML response: %s at %d:%d!", doc.ErrorDesc(), doc.ErrorRow(), doc.ErrorCol());
			Scan->pres->std_reply(Scan->pres, buf);
			remove(fn);
		}
		zfree(file);
	} else {
		snprintf(buf, sizeof(buf), "Error downloading definitions: %s!", errbuf);
		Scan->pres->std_reply(Scan->pres, buf);
	}
}
