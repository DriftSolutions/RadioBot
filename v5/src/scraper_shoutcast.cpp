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
bool Prepare_SC(int num, BUFFER * buf) {
	std::stringstream sstr,sstr2;
	sstr << "http://" << config.s_servers[num].host << ":" << config.s_servers[num].port << "/";
	if (config.s_servers[num].pass.length()) {
		sstr << "admin.cgi?mode=viewxml&page=1";
		sstr2 << "admin:" << config.s_servers[num].pass;
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
 * Parses the SHOUTcast server response (status page for if no password is given) for stream info.<br>
 * Returns true if successful, false otherwise.
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER structure the response and CURL handle are stored in
 * @param stats the STATS structure to fill in
 */
bool Parse_SC_NoPass(int num, BUFFER * buf, STATS * stats) {
	bool ret = false;
	memset(stats, 0, sizeof(STATS));

	DRIFT_DIGITAL_SIGNATURE();
	char *tmp = buf->data ? strstr(buf->data,"Stream is up at "):NULL;
	if (tmp) {
		if (access("shoutcast.txt", 0) == 0) { remove("shoutcast.txt"); }
#if defined(DEBUG)
		FILE * fp = fopen("shoutcast.txt", "wb");
		if (fp) {
			fwrite(buf->data, buf->len, 1, fp);
			fclose(fp);
		}
#endif

		stats->online = true;
		tmp += strlen("Stream is up at ");
		sscanf(tmp,"%d kbps with <B>%d of %d listeners",&stats->bitrate,&stats->listeners,&stats->maxusers);

		tmp = strstr(buf->data,"Listener Peak: </font></td><td><font class=default><b>");
		if (tmp) {
			sscanf(tmp,"Listener Peak: </font></td><td><font class=default><b>%d</b>",&stats->peak);
		} else {
			ib_printf(_("SS Scraper -> Server[%d] has no Listener Peak tag!\n"));
		}

		ib_printf(_("SS Scraper -> Query of server[%d]: %d/%d/%d/%d\n"),num,stats->bitrate,stats->listeners,stats->maxusers,stats->peak);
		tmp = strstr(buf->data,"Stream Title: </font></td><td><font class=default><b>");
		if (tmp) {
			tmp += strlen("Stream Title: </font></td><td><font class=default><b>");
			strncpy(stats->curdj,tmp,strstr(tmp,"</b>") - tmp);
			stats->curdj[strstr(tmp,"</b>") - tmp]=0;
		} else {
			ib_printf(_("SS Scraper -> Server[%d] has no Stream Title tag!\n"));
		}

		tmp = strstr(buf->data,"Current Song: </font></td><td><font class=default><b>");
		if (tmp) {
			tmp += strlen("Current Song: </font></td><td><font class=default><b>");
			strncpy(stats->cursong,tmp,strstr(tmp,"</b>") - tmp);
			stats->cursong[strstr(tmp,"</b>") - tmp]=0;
		} else {
			ib_printf(_("SS Scraper -> Server[%d] has no Current Song tag!\n"));
		}

		ib_printf(_("SS Scraper -> DJ: %s\n"),stats->curdj);
		ib_printf(_("SS Scraper -> Song: %s\n"),stats->cursong);
		ret = true;
	} else {
		if (strstr(buf->data,"Server is currently down") != NULL) {
			ib_printf(_("SS Scraper -> Server [%d] currently has no source!\n"),num);
			ret = true;
		} else {
			ib_printf(_("SS Scraper -> Got garbage from server [%d]\n"),num);
		}
	}
	return ret;
}

/**
 * Parses the SHOUTcast server response (XML w/password) for stream info.<br>
 * Returns true if successful, false otherwise.
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER structure the response and CURL handle are stored in
 * @param stats the STATS structure to fill in
 */
bool Parse_SC(int num, BUFFER * buf, STATS * stats) {
	if (config.s_servers[num].pass.length() == 0) {
		return Parse_SC_NoPass(num, buf, stats);
	}

	bool ret = false;
	memset(stats, 0, sizeof(STATS));

	char * p = stristr(buf->data, "<SHOUTCASTSERVER>");
	if (p) {
		TiXmlDocument * doc = new TiXmlDocument();
		doc->Parse(p);
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
	} else {
		ib_printf(_("SS Scraper -> Got garbage from server [%d]\n"),num);
	}

	return ret;
}

#endif
