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

#include "ircbot.h"
#if defined(IRCBOT_ENABLE_SS)

/**
 * Prepares a CURL handle for scraping an steamcast server.<br>
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER sctructure to store the handle in
 */
bool Prepare_Steamcast(int num, BUFFER * buf) {
	std::stringstream sstr, sstr2;

	if (config.s_servers[num].user.length() == 0 || config.s_servers[num].pass.length() == 0) {
		ib_printf("[ss-%d] Steamcast server has no user/pass defined!\r\n", num);
		return false;
	} else {
		sstr << "http://" << config.s_servers[num].host << ":" << config.s_servers[num].port << "/admin/status.xml";
		sstr2 << config.s_servers[num].user << ":" << config.s_servers[num].pass;
		curl_easy_setopt(buf->handle, CURLOPT_URL, sstr.str().c_str());
#if defined(OLD_CURL)
		buf->url = zstrdup(sstr.str().c_str());
		curl_easy_setopt(buf->handle, CURLOPT_URL, buf->url);
		buf->userpwd = zstrdup(sstr2.str().c_str());
		curl_easy_setopt(buf->handle, CURLOPT_USERPWD, buf->userpwd);
#else
		curl_easy_setopt(buf->handle, CURLOPT_URL, sstr.str().c_str());
		curl_easy_setopt(buf->handle, CURLOPT_USERPWD, sstr2.str().c_str());
#endif
		return true;
	}
}

/**
 * Parses the response from an steamcast server for stream info.<br>
 * Returns true if successful, false otherwise.
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER structure the response and CURL handle are stored in
 * @param stats the STATS structure to fill in
 */
bool Parse_Steamcast(int num, BUFFER * buf, STATS * stats) {
	bool ret = false;
	memset(stats, 0, sizeof(STATS));

	TiXmlDocument * doc = new TiXmlDocument();
	char * p = stristr(buf->data, "<steamcast>");
	doc->Parse(p ? p:buf->data);
	//doc->Print(stdout);
	TiXmlElement * root = doc->FirstChildElement("steamcast");
	if (root) {
		ret = true;
		root = root->FirstChildElement("sources");
		if (root) {
			TiXmlElement * m = root->FirstChildElement("source");
			while (m) {
				char buf2[512];
				IfExistCopy(m, "mount", buf2, sizeof(buf2));
				if (!stricmp(buf2, config.s_servers[num].mount.c_str())) {
					int tmp = IfExistInt(m, "status", -1);
					if (tmp == 1) {
						stats->online = true;
						IfExistCopy(m, "name", stats->curdj, sizeof(stats->curdj));
						IfExistCopy(m, "meta_song", stats->cursong, sizeof(stats->cursong));
						stats->listeners = IfExistInt(m, "nodes");
						stats->peak = IfExistInt(m, "listener_peak");
						stats->maxusers = IfExistInt(m, "max_nodes");
						stats->bitrate = IfExistInt(m, "bitrate");

						ib_printf(_("SS Scraper -> Query of server[%d]: %d/%d/%d/%d\n"),num,stats->bitrate,stats->listeners,stats->maxusers,stats->peak);
						ib_printf(_("SS Scraper -> DJ: %s\n"),stats->curdj);
						ib_printf(_("SS Scraper -> Song: %s\n"),stats->cursong);
					}
					break;
				}
				m = m->NextSiblingElement("source");
			}
			if (!stats->online) {
				ib_printf(_("SS Scraper -> Server [%d] currently has no source!\n"), num);
			}
		} else {
			ib_printf(_("SS Scraper -> Got garbage from server [%d]\n"),num);
		}
	} else {
		ib_printf(_("SS Scraper -> Got garbage from server [%d]\n"),num);
	}
	delete doc;

	//DRIFT_DIGITAL_SIGNATURE();
	return ret;
}

#endif
