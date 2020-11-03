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

#include "liblastfm.h"

size_t curl_write(void *ptr, size_t size, size_t nmemb, void *stream) {
	return fwrite(ptr, size, nmemb, (FILE *)stream);
}

bool lfm_handshake() {
	api->ib_printf("Last.FM Support -> Handshaking with Last.FM server...\n");

	std::ostringstream sstr;
	bool ret = false;
	char buf[1024],buf2[128],token[128];
	memset(buf, 0, sizeof(buf));
	memset(buf2, 0, sizeof(buf2));
	memset(token, 0, sizeof(token));
	//md5((unsigned char *)lfm_config.pass, strlen(lfm_config.pass), buf);
	hashdata("md5", (unsigned char *)lfm_config.pass, strlen(lfm_config.pass), buf, sizeof(buf));
	int64 tme = time(NULL);
	sprintf(buf2, I64FMT, tme);
	strcat(buf, buf2);
	hashdata("md5", (unsigned char *)buf, strlen(buf), token, sizeof(token));
	//md5((unsigned char *)buf, strlen(buf), token);
	char * user = api->curl->escape(lfm_config.user, strlen(lfm_config.user));
	sstr << "http://post.audioscrobbler.com/?hs=true&p=1.2.1&c=" << CLIENT << "&v=" << VERSION << "&u=" << user << "&t=" << tme << "&a=" << token;
	api->curl->free(user);

	CURL * h = api->curl->easy_init();
	if (h != NULL) {
		FILE * fp = fopen("./tmp/lastfm.tmp", "wb");
		if (fp != NULL) {
			char errbuf[CURL_ERROR_SIZE+1];
			memset(errbuf, 0, sizeof(errbuf));
			api->curl->easy_setopt(h, CURLOPT_URL, sstr.str().c_str());
			api->curl->easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);
			api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
			api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 60);
			api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10);
			api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1);
			api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, curl_write);
			api->curl->easy_setopt(h, CURLOPT_WRITEDATA, fp);
			if (api->curl->easy_perform(h) == CURLE_OK) {
				fclose(fp);
				fp = fopen("./tmp/lastfm.tmp", "rb");
				if (fp != NULL) {
					memset(buf, 0, sizeof(buf));
					if (fgets(buf, sizeof(buf), fp) == NULL) {
						api->ib_printf("Last.FM Support -> Error reading server reply from temp file!\n");
					}
					strtrim(buf);
					if (stricmp(buf, "OK") == 0) {
						memset(buf, 0, sizeof(buf));
						if (fgets(buf, sizeof(buf), fp) == NULL) {
							api->ib_printf("Last.FM Support -> Error reading session ID from temp file!\n");
						}
						strtrim(buf);
						sstrcpy(lfm_config.sesid, buf);
						memset(buf, 0, sizeof(buf));
						if (fgets(buf, sizeof(buf), fp) == NULL) {
							api->ib_printf("Last.FM Support -> Error reading NP URL from temp file!\n");
						}
						strtrim(buf);
						sstrcpy(lfm_config.np_url, buf);
						memset(buf, 0, sizeof(buf));
						if (fgets(buf, sizeof(buf), fp) == NULL) {
							api->ib_printf("Last.FM Support -> Error reading SUB URL from temp file!\n");
						}
						strtrim(buf);
						sstrcpy(lfm_config.sub_url, buf);
						ret = true;
						lfm_config.handshaked = true;
						api->ib_printf("Last.FM Support -> Handshake complete!\n");
					} else if (stricmp(buf, "BANNED") == 0) {
						api->ib_printf("Last.FM Support -> Error handshaking, a new version of the plugin will be needed in order for Last.FM to continue working!\n");
					} else if (stricmp(buf, "BADAUTH") == 0) {
						api->ib_printf("Last.FM Support -> Error handshaking, invalid username and/or password!\n");
					} else if (stricmp(buf, "BADTIME") == 0) {
						api->ib_printf("Last.FM Support -> Error handshaking, bad timestamp. Make sure your PC's clock is set to the correct date & time.\n");
					} else if (strnicmp(buf, "FAILED ", 7) == 0) {
						api->ib_printf("Last.FM Support -> Error handshaking: %s\n", buf);
					}
				} else {
					api->ib_printf("Last.FM Support -> Error re-opening temp file!\n");
				}
			} else {
				api->ib_printf("Last.FM Support -> Error communicating with Last.FM server: %s!\n", errbuf);
			}
			fclose(fp);
			remove("./tmp/lastfm.tmp");
		} else {
			api->ib_printf("Last.FM Support -> Error opening temp file!\n");
		}
		api->curl->easy_cleanup(h);
	} else {
		api->ib_printf("Last.FM Support -> Error creating CURL handle!\n");
	}
	return ret;
}

bool lfm_scrobble(const char * artist, const char * album, const char * title, int songLen, int64 startTime) {
	if (!lfm_config.handshaked) {
		if (!lfm_handshake()) {
			return false;
		}
	}

	api->ib_printf("Last.FM Support -> Sending song information...\n");

	bool ret = false;
	char buf[1024],buf2[128];
	memset(buf, 0, sizeof(buf));

	CURL * h = api->curl->easy_init();
	if (h != NULL) {
		FILE * fp = fopen("./tmp/lastfm.tmp", "wb");
		if (fp != NULL) {
			char errbuf[CURL_ERROR_SIZE+1];
			memset(errbuf, 0, sizeof(errbuf));
			api->curl->easy_setopt(h, CURLOPT_URL, lfm_config.sub_url);
			api->curl->easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);
			api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
			api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1);
			api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 60);
			api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10);
			api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, curl_write);
			api->curl->easy_setopt(h, CURLOPT_WRITEDATA, fp);
			api->curl->easy_setopt(h, CURLOPT_POST, 1);

			Titus_Buffer tb;
			char * p = api->curl->escape(lfm_config.sesid, 0);
			tb.Append("s=");
			tb.Append(p);
			api->curl->free(p);
			p = api->curl->escape(artist?artist:"", 0);
			tb.Append("&a[0]=");
			tb.Append(p);
			api->curl->free(p);
			p = api->curl->escape(title, 0);
			tb.Append("&t[0]=");
			tb.Append(p);
			api->curl->free(p);

			sprintf(buf2, I64FMT, startTime);
			p = api->curl->escape(buf2, 0);
			tb.Append("&i[0]=");
			tb.Append(p);
			api->curl->free(p);
			tb.Append("&o[0]=P");
			sprintf(buf2, "%d", songLen);
			p = api->curl->escape(buf2, 0);
			tb.Append("&l[0]=");
			tb.Append(p);
			api->curl->free(p);
			p = api->curl->escape(album?album:"", 0);
			tb.Append("&b[0]=");
			tb.Append(p);
			api->curl->free(p);
			tb.Append("&r[0]=&n[0]=&m[0]=");
			tb.Append_int8(0);

			/*
			struct curl_httppost* post = NULL;
			struct curl_httppost* last = NULL;

			api->curl->formadd(&post, &last, CURLFORM_COPYNAME, "s", CURLFORM_COPYCONTENTS, lfm_config.sesid, CURLFORM_END);
			api->curl->formadd(&post, &last, CURLFORM_COPYNAME, "a[0]", CURLFORM_COPYCONTENTS, artist?artist:"", CURLFORM_END);
			api->curl->formadd(&post, &last, CURLFORM_COPYNAME, "t[0]", CURLFORM_COPYCONTENTS, title, CURLFORM_END);
			if (sizeof(time_t) == 8) {
				sprintf(buf2, I64FMT, startTime);
			} else {
				sprintf(buf2, "%d", startTime);
			}
			api->curl->formadd(&post, &last, CURLFORM_COPYNAME, "i[0]", CURLFORM_COPYCONTENTS, buf2, CURLFORM_END);
			api->curl->formadd(&post, &last, CURLFORM_COPYNAME, "o[0]", CURLFORM_COPYCONTENTS, "P", CURLFORM_END);
			sprintf(buf2, "%d", songLen);
			api->curl->formadd(&post, &last, CURLFORM_COPYNAME, "l[0]", CURLFORM_COPYCONTENTS, buf2, CURLFORM_END);
			api->curl->formadd(&post, &last, CURLFORM_COPYNAME, "b[0]", CURLFORM_COPYCONTENTS, album?album:"", CURLFORM_END);
			api->curl->easy_setopt(h, CURLOPT_HTTPPOST, post);
			*/

			api->curl->easy_setopt(h, CURLOPT_POSTFIELDS, tb.Get());

			if (api->curl->easy_perform(h) == CURLE_OK) {
				fclose(fp);
				fp = fopen("./tmp/lastfm.tmp", "rb");
				if (fp != NULL) {
					memset(buf, 0, sizeof(buf));
					if (fgets(buf, sizeof(buf), fp) == NULL) {
						api->ib_printf("Last.FM Support -> Error reading reply from temp file!\n");
					}
					strtrim(buf);
					if (stricmp(buf, "OK") == 0) {
						api->ib_printf("Last.FM Support -> Song submitted!\n");
						ret = true;
					} else if (stricmp(buf, "BADSESSION") == 0) {
						lfm_config.handshaked = false;
					} else if (strnicmp(buf, "FAILED ", 7) == 0) {
						api->ib_printf("Last.FM Support -> Error submitting song: %s\n", buf);
					}
				} else {
					api->ib_printf("Last.FM Support -> Error re-opening temp file!\n");
				}
			} else {
				api->ib_printf("Last.FM Support -> Error communicating with Last.FM server: %s!\n", errbuf);
			}
			//api->curl->formfree(post);
			fclose(fp);
			remove("./tmp/lastfm.tmp");
		} else {
			api->ib_printf("Last.FM Support -> Error opening temp file!\n");
		}
		api->curl->easy_cleanup(h);
	} else {
		api->ib_printf("Last.FM Support -> Error creating CURL handle!\n");
	}
	return ret;
}
