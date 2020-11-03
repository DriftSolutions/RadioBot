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

CURL * CreateYPHandle(BUFFER * pbuf) {
	CURL * ret = curl_easy_init();
	if (ret != NULL) {
		char buf[128];
		curl_easy_setopt(ret, CURLOPT_TIMEOUT, 60);
		curl_easy_setopt(ret, CURLOPT_CONNECTTIMEOUT, 10);
		curl_easy_setopt(ret, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(ret, CURLOPT_NOSIGNAL, 1 );
		curl_easy_setopt(ret, CURLOPT_WRITEFUNCTION, buffer_write);
		curl_easy_setopt(ret, CURLOPT_WRITEDATA, pbuf);
		snprintf(buf, sizeof(buf), "RadioBot v%s - www.shoutirc.com (Mozilla)", GetBotVersionString());
		curl_easy_setopt(ret, CURLOPT_USERAGENT, buf);
	}
	return ret;
}


bool AddToYP(YP_HANDLE * yp, SOUND_SERVER * sc, YP_CREATE_INFO * ypinfo) {
	BUFFER buf;
	memset(&buf, 0, sizeof(buf));
	CURL * h = CreateYPHandle(&buf);
	if (h == NULL) {
		return false;
	}

	bool ret = false;

	std::stringstream sstr;
	char * host = curl_escape(sc->host, strlen(sc->host));
	sstr << "http://www.shoutirc.com/yp.php?action=add&host=" << host << "&port=" << sc->port << "&type=" << sc->type;
	curl_free(host);
	host = curl_escape(ypinfo->mime_type, strlen(ypinfo->mime_type));
	sstr << "&mime=" << host;
	curl_free(host);
	host = curl_escape(ypinfo->source_name, strlen(ypinfo->source_name));
	sstr << "&source=" << host;
	curl_free(host);
#if defined(IRCBOT_ENABLE_IRC)
	host = curl_escape(GetDefaultNick(-1), strlen(GetDefaultNick(-1)));
#else
	host = curl_escape("AutoDJ", 6);
#endif
	sstr << "&nick=" << host;
	curl_free(host);
	sstr << "&sid=" << sc->streamid;
	host = curl_escape(sc->mount, strlen(sc->mount));
	sstr << "&mount=" << host;
	curl_free(host);
	host = curl_escape(ypinfo->genre, strlen(ypinfo->genre));
	sstr << "&genre=" << host;
	curl_free(host);
	host = curl_escape(ypinfo->cur_playing, strlen(ypinfo->cur_playing));
	sstr << "&title=" << host;
	curl_free(host);

#if defined(OLD_CURL)
	buf.url = zstrdup(sstr.str().c_str());
	curl_easy_setopt(h, CURLOPT_URL, buf.url);
#else
	curl_easy_setopt(h, CURLOPT_URL, sstr.str().c_str());
#endif

#if defined(DEBUG)
	printf("YP-URL: %s\n", sstr.str().c_str());
#endif
	CURLcode dret = curl_easy_perform(h);
	if (dret == CURLE_OK) {
		if (!strnicmp(buf.data, "OK ", 3)) {
			char *p2=NULL;
			char *p = strtok_r(buf.data, " \r\n", &p2);
			p = strtok_r(NULL, " \r\n", &p2);
			yp->yp_id = atoul(p ? p:"0");
			if (p && yp->yp_id) {
				p = strtok_r(NULL, " \r\n", &p2);
				if (p) {
					strlcpy(yp->key, p, sizeof(yp->key));
				}
				ib_printf(_("%s: Successfully added to the ShoutIRC.com YP\n"), IRCBOT_NAME);
				ret = true;
			} else {
				ib_printf(_("%s: Error adding myself to the ShoutIRC.com YP: unrecognized response\n"), IRCBOT_NAME);
			}
		} else {
			ib_printf(_("%s: Error adding myself to the ShoutIRC.com YP: %s\n"), IRCBOT_NAME, buf.data);
		}
	} else {
		ib_printf(_("%s: Error adding myself to the ShoutIRC.com YP: %s\n"), IRCBOT_NAME, curl_easy_strerror(dret));
	}

	curl_easy_cleanup(h);
	zfreenn(buf.data);
#if defined(OLD_CURL)
	if (buf.url) { zfree(buf.url); }
#endif
	return ret;
}

int TouchYP(YP_HANDLE * yp, const char * cur_playing) {
	BUFFER buf;
	memset(&buf, 0, sizeof(buf));
	CURL * h = CreateYPHandle(&buf);
	if (h == NULL) {
		return -1;
	}

	std::stringstream sstr;
	sstr << "http://www.shoutirc.com/yp.php?action=touch2&id=" << yp->yp_id << "&key=" << yp->key;
	if (cur_playing && cur_playing[0]) {
		char * title = curl_escape(cur_playing, strlen(cur_playing));
		sstr << "&title=" << title;
		curl_free(title);
	}

#if defined(OLD_CURL)
	buf.url = zstrdup(sstr.str().c_str());
	curl_easy_setopt(h, CURLOPT_URL, buf.url);
#else
	curl_easy_setopt(h, CURLOPT_URL, sstr.str().c_str());
#endif

	CURLcode dret = curl_easy_perform(h);
	int ret = -1;
	if (dret == CURLE_OK) {
		if (!strnicmp(buf.data, "OK", 2)) {
			ib_printf(_("%s: Successfully updated to the ShoutIRC.com YP\n"), IRCBOT_NAME);
			ret = 0;
		} else {
			ib_printf(_("%s: Error updating to the ShoutIRC.com YP: %s\n"), IRCBOT_NAME, buf.data);
			ret = -2;
		}
	} else {
		ib_printf(_("%s: Error updating to the ShoutIRC.com YP: %s\n"), IRCBOT_NAME, curl_easy_strerror(dret));
	}

	curl_easy_cleanup(h);
	zfreenn(buf.data);
#if defined(OLD_CURL)
	if (buf.url) { zfree(buf.url); }
#endif
	return ret;
}

void DelFromYP(YP_HANDLE * yp) {
	BUFFER buf;
	memset(&buf, 0, sizeof(buf));
	CURL * h = CreateYPHandle(&buf);
	if (h == NULL) {
		return;
	}

	std::stringstream sstr;
	sstr << "http://www.shoutirc.com/yp.php?action=remove2&id=" << yp->yp_id << "&key=" << yp->key;

#if defined(OLD_CURL)
	buf.url = zstrdup(sstr.str().c_str());
	curl_easy_setopt(h, CURLOPT_URL, buf.url);
#else
	curl_easy_setopt(h, CURLOPT_URL, sstr.str().c_str());
#endif

	CURLcode dret = curl_easy_perform(h);
	if (dret == CURLE_OK) {
		if (!strnicmp(buf.data, "OK", 2)) {
			ib_printf(_("%s: Successfully removed myself from the ShoutIRC.com YP\n"), IRCBOT_NAME);
		} else {
			ib_printf(_("%s: Error removing myself from the ShoutIRC.com YP: %s\n"), IRCBOT_NAME, buf.data);
		}
	} else {
		ib_printf(_("%s: Error removing myself from the ShoutIRC.com YP: %s\n"), IRCBOT_NAME, curl_easy_strerror(dret));
	}

	curl_easy_cleanup(h);
	zfreenn(buf.data);
#if defined(OLD_CURL)
	if (buf.url) { zfree(buf.url); }
#endif
}
