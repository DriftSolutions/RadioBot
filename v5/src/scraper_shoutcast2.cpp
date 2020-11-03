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
 * Prepares a CURL handle for scraping a SHOUTcast server.<br>
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER sctructure to store the handle in
 */
bool Prepare_SC2(int num, BUFFER * buf) {
	std::stringstream sstr,sstr2;
	sstr << "http://" << config.s_servers[num].host << ":" << config.s_servers[num].port << "/";
	if (config.s_servers[num].pass.length()) {
		sstr << "admin.cgi?sid=" << config.s_servers[num].streamid << "&mode=viewxml&page=1";
		sstr2 << "admin:" << config.s_servers[num].pass;
	} else {
		sstr << "stats?sid=" << config.s_servers[num].streamid;
	}

#if defined(OLD_CURL)
	buf->url = zstrdup(sstr.str().c_str());
	curl_easy_setopt(buf->handle, CURLOPT_URL, buf->url);
	if (sstr2.str().length()) {
		buf->userpwd = zstrdup(sstr2.str().c_str());
		curl_easy_setopt(buf->handle, CURLOPT_USERPWD, buf->userpwd);
	}
#else
	curl_easy_setopt(buf->handle, CURLOPT_URL, sstr.str().c_str());
	if (sstr2.str().length()) {
		curl_easy_setopt(buf->handle, CURLOPT_USERPWD, sstr2.str().c_str());
	}
#endif
	return true;
};

/**
 * Parses the SHOUTcast server response (XML with or without password) for stream info.<br>
 * Returns true if successful, false otherwise.
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER structure the response and CURL handle are stored in
 * @param stats the STATS structure to fill in
 */
bool Parse_SC2(int num, BUFFER * buf, STATS * stats) {
	//if (config.s_servers[num].pass.length() == 0) {
	//	return Parse_SC2_NoPass(num, buf, stats);
	//}
	bool ret = false;
	memset(stats, 0, sizeof(STATS));

	TiXmlDocument * doc = new TiXmlDocument();
	doc->Parse(buf->data);
	TiXmlElement * root = doc->FirstChildElement("SHOUTCASTSERVER");
	if (root) {
		ret = true;
		if (IfExistInt(root, "STREAMSTATUS", 0) == 1) {
			stats->online = true;
			IfExistCopy(root, "SERVERTITLE", stats->curdj, sizeof(stats->curdj));
			strtrim(stats->curdj);
			IfExistCopy(root, "SONGTITLE", stats->cursong, sizeof(stats->cursong));
			strtrim(stats->cursong);
			stats->listeners = IfExistInt(root, "CURRENTLISTENERS");
			stats->peak = IfExistInt(root, "PEAKLISTENERS");
			stats->maxusers = IfExistInt(root, "MAXLISTENERS");
			stats->bitrate = IfExistInt(root, "BITRATE");

			ib_printf(_("SS Scraper -> Query of server[%d]: %d/%d/%d/%d\n"),num,stats->bitrate,stats->listeners,stats->maxusers,stats->peak);
			ib_printf(_("SS Scraper -> DJ: %s\n"),stats->curdj);
			ib_printf(_("SS Scraper -> Song: %s\n"),stats->cursong);
		} else {
			ib_printf(_("SS Scraper -> Server [%d] currently has no source!\n"), num);
		}
	} else {
		ib_printf(_("SS Scraper -> Got garbage from server [%d]\n"),num);
	}
	delete doc;

	return ret;
}

#endif
