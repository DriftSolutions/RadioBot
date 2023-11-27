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
 * Prepares a CURL handle for scraping an icecast2 server.<br>
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER sctructure to store the handle in
 */
bool Prepare_ICE(int num, BUFFER * buf) {
	std::stringstream sstr, sstr2;

	if (config.s_servers[num].user.length() == 0 && config.s_servers[num].pass.length() == 0) {
		sstr << "http://" << config.s_servers[num].host << ":" << config.s_servers[num].port << "/status.xsl";
#if defined(OLD_CURL)
		buf->url = zstrdup(sstr.str().c_str());
		curl_easy_setopt(buf->handle, CURLOPT_URL, buf->url);
#else
		curl_easy_setopt(buf->handle, CURLOPT_URL, sstr.str().c_str());
#endif
	} else {
		sstr << "http://" << config.s_servers[num].host << ":" << config.s_servers[num].port << "/admin/stats.xml";
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
	}
	return true;
}

/*
<mounts>
  <mount>
    <mountpoint>/music.mp3</mountpoint>
    <name>DJ Test</name>
    <description>CrystalOne Test Stream</description>
    <title>Follow The Leader.mp3</title>
    <url>http://www.crystalone.net</url>

    <listeners>0</listeners>
    <peak>0</peak>
    <max>unlimited</max>
    <bitrate>64</bitrate>
  </mount>
</mounts>
*/

/**
 * Get the value of the child element name in m (if it exists).<br>
 * Returns true if the value exists and was copied, false otherwise.
 * @param m the parent TiXmlElement
 * @param name the name of the child item to find
 * @param buf the user-supplied buffer to store the value in
 * @param bufSize the size of buf
 */
bool IfExistCopy(TiXmlElement * m, char * name, char * buf, int bufSize) {
	TiXmlElement * e = m->FirstChildElement(name);
	if (e) {
		const char * p = e->GetText();
		if (p) {
			strlcpy(buf, p, bufSize);
			return true;
		}
	}
	return false;
}

/**
 * Get the numeric value of the child element name in m (if it exists).<br>
 * Returns nDef if the child isn't found, otherwise the numeric value of the child.
 * @param m the parent TiXmlElement
 * @param name the name of the child item to find
 * @param nDef the default value to return if the child is not found. (default: 0)
 */
int IfExistInt(TiXmlElement * m, char * name, int nDef) {
	int ret = nDef;
	TiXmlElement * e = m->FirstChildElement(name);
	if (e) {
		const char * p = e->GetText();
		if (p) {
			if (!stricmp(p, "unlimited")) {
				ret = 999;
			} else {
				ret = atoi(p);
			}
		}
	}
	return ret;
}

/**
 * Parses the response from an icecast2 server for stream info.<br>
 * Returns true if successful, false otherwise.
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER structure the response and CURL handle are stored in
 * @param stats the STATS structure to fill in
 */
bool Parse_ICE_NoPass(int num, BUFFER * buf, STATS * stats) {
	bool ret = false;
	memset(stats, 0, sizeof(STATS));
	TiXmlDocument * doc = new TiXmlDocument();
	doc->Parse(buf->data);
	TiXmlElement * root = doc->FirstChildElement("mounts");
	if (root) {
		ret = true;
		TiXmlElement * m = root->FirstChildElement("mount");
		while (m) {
			TiXmlElement * e = m->FirstChildElement("mountpoint");
			const char * p = NULL;
			if (e) {
				p = e->GetText();
				if (p && !stricmp(p, config.s_servers[num].mount.c_str())) {
					stats->online = true;
					IfExistCopy(m, "name", stats->curdj, sizeof(stats->curdj));
					char buf2[512];
					memset(buf2, 0, sizeof(buf2));
					IfExistCopy(m, "artist", buf2, sizeof(buf2));
					if (strlen(buf2)) {
						char buf3[512];
						memset(buf3, 0, sizeof(buf3));
						IfExistCopy(m, "title", buf3, sizeof(buf3));
						snprintf(stats->cursong, sizeof(stats->cursong), "%s - %s", buf2, buf3);
					} else {
						IfExistCopy(m, "title", stats->cursong, sizeof(stats->cursong));
					}
					stats->listeners = IfExistInt(m, "listeners");
					stats->peak = IfExistInt(m, "peak");
					stats->maxusers = IfExistInt(m, "max");
					if (stats->maxusers < 0) { stats->maxusers = 999; }
					stats->bitrate = IfExistInt(m, "bitrate");

					ib_printf(_("SS Scraper -> Query of server[%d]: %d/%d/%d/%d\n"),num,stats->bitrate,stats->listeners,stats->maxusers,stats->peak);
					ib_printf(_("SS Scraper -> DJ: %s\n"),stats->curdj);
					ib_printf(_("SS Scraper -> Song: %s\n"),stats->cursong);
					break;
				}
			}
			m = m->NextSiblingElement();
		}
	} else {
		ib_printf(_("SS Scraper -> Got garbage from server [%d]\n"),num);
	}
	delete doc;
	return ret;
}

/**
 * Parses the response from an icecast2 server for stream info.<br>
 * Returns true if successful, false otherwise.
 * @param num the number of the SS server to scrape
 * @param buf the BUFFER structure the response and CURL handle are stored in
 * @param stats the STATS structure to fill in
 */
bool Parse_ICE(int num, BUFFER * buf, STATS * stats) {
	if (config.s_servers[num].user.length() == 0 && config.s_servers[num].pass.length() == 0) {
		return Parse_ICE_NoPass(num, buf, stats);
	}

	bool ret = false;
	memset(stats, 0, sizeof(STATS));

	TiXmlDocument * doc = new TiXmlDocument();
	doc->Parse(buf->data);
	TiXmlElement * root = doc->FirstChildElement("icestats");
	if (root) {
		ret = true;
		TiXmlElement * m = root->FirstChildElement("source");
		while (m) {
			const char * p = m->Attribute("mount");
			if (p && !stricmp(p, config.s_servers[num].mount.c_str())) {
				//p = e->GetText();
				//if (p && !stricmp(p, config.s_servers[num].mount.c_str())) {
					stats->online = false;
					IfExistCopy(m, "source_ip", stats->curdj, sizeof(stats->curdj));
					if (stats->curdj[0]) {
						stats->online = true;
					}
					stats->curdj[0] = 0;
					IfExistCopy(m, "server_name", stats->curdj, sizeof(stats->curdj));
					char buf2[512];
					memset(buf2, 0, sizeof(buf2));
					IfExistCopy(m, "artist", buf2, sizeof(buf2));
					if (strlen(buf2)) {
						char buf3[512];
						memset(buf3, 0, sizeof(buf3));
						IfExistCopy(m, "title", buf3, sizeof(buf3));
						snprintf(stats->cursong, sizeof(stats->cursong), "%s - %s", buf2, buf3);
					} else {
						IfExistCopy(m, "title", stats->cursong, sizeof(stats->cursong));
					}
					stats->listeners = IfExistInt(m, "listeners");
					stats->peak = IfExistInt(m, "listener_peak");
					stats->maxusers = IfExistInt(m, "max_listeners");
					stats->bitrate = IfExistInt(m, "ice-bitrate");

					if (stats->online) {
						ib_printf(_("SS Scraper -> Query of server[%d]: %d/%d/%d/%d\n"),num,stats->bitrate,stats->listeners,stats->maxusers,stats->peak);
						ib_printf(_("SS Scraper -> DJ: %s\n"),stats->curdj);
						ib_printf(_("SS Scraper -> Song: %s\n"),stats->cursong);
					}
					break;
				//}
			}
			m = m->NextSiblingElement("source");
		}
		if (!stats->online) {
			ib_printf(_("SS Scraper -> Server [%d] currently has no source!\n"), num);
		}
	} else {
		ib_printf(_("SS Scraper -> Got garbage from server [%d]\n"),num);
	}
	delete doc;

	//DRIFT_DIGITAL_SIGNATURE();
	return ret;
}

#endif
