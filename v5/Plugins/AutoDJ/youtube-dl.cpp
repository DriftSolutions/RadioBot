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

#include "autodj.h"

#include <drift/SyncedInt.h>

SyncedInt<uint32> ytDownloading;

int get_url_cb(void * parm, int ncols, char ** row, char ** cols) {
	char * fn = (char *)parm;
	if (row[0]) {
		strcpy(fn, row[0]);
	}
	return 0;
}

size_t cachedDownloadWrite(void *ptr, size_t size, size_t nmemb, void *stream) {
	FILE * fp = (FILE *)stream;
	return fwrite(ptr, size, nmemb, fp);
};

char * get_cached_download(const char * url, const char * fn, char * error, size_t errSize) {//, CACHED_DOWNLOAD_OPTIONS * opts
	char * ret = NULL;

	struct stat st;
	if (stat(fn, &st) == 0) {
		time_t timeLimit = 300;
		if ((time(NULL) - st.st_mtime) < timeLimit) {
			FILE * fp = fopen(fn, "rb");
			if (fp) {
				long len = st.st_size;
				ret = (char *)zmalloc(len + 1);
				ret[len] = 0;
				if (fread(ret, len, 1, fp) != 1) {
					zfree(ret);
					ret = NULL;
				}
				fclose(fp);
			}
		}
	}

	if (ret == NULL) {
		FILE * fp = fopen(fn, "w+b");
		if (fp) {
			CURL * h = api->curl->easy_init();
			if (h) {
				api->curl->easy_setopt(h, CURLOPT_URL, url);
				api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
				api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1);
				api->curl->easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1);
				api->curl->easy_setopt(h, CURLOPT_MAXREDIRS, 3);
				api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 60);
				api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10);
				api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, cachedDownloadWrite);
				api->curl->easy_setopt(h, CURLOPT_WRITEDATA, fp);
				api->curl->easy_setopt(h, CURLOPT_FAILONERROR, 0);
				api->curl->easy_setopt(h, CURLOPT_ERRORBUFFER, error);
				api->curl->easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0);
				api->curl->easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0);
				api->curl->easy_setopt(h, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);

				api->ib_printf(_("AutoDJ -> Performing URL fetch...\n"));
				CURLcode res = api->curl->easy_perform(h);
				if (res == CURLE_OK) {
					long len = ftell(fp);
					fseek(fp, 0, SEEK_SET);
					ret = (char *)zmalloc(len + 1);
					ret[len] = 0;
					if (fread(ret, len, 1, fp) != 1) {
						snprintf(error, errSize, "error reading file");
						zfree(ret);
						ret = NULL;
					}
				} else {
					snprintf(error, errSize, "cURL Error: %s", api->curl->easy_strerror(res));
				}
				api->curl->easy_cleanup(h);
			} else {
				strlcpy(error, "error allocating curl handle", errSize);
			}
			fclose(fp);
			if (ret == NULL) { remove(fn); }
		} else {
			strlcpy(error, "error allocating temp file", errSize);
		}
	}

	return ret;
}

struct YOUTUBESEARCH {
	char video_url[256];
	char video_id[256];
	char title[256];
	char poster[256];
};

bool get_video_id(USER_PRESENCE * ut, const char * query, YOUTUBESEARCH& y) {
	char buf[2048], errbuf[1024];
	char key[64];
	hashdata("md5", (uint8 *)query, strlen(query), key, sizeof(key));

	char fn[1024];
	sprintf(fn, "./tmp/youtubesearch.%s.tmp", key);
	stringstream url;
	buf[0] = 0;
#define HMAC_KEY "zN!#tbjzQRjsCEll"
	hmacdata("sha256", (uint8 *)HMAC_KEY, strlen(HMAC_KEY), (uint8 *)query, strlen(query), buf, sizeof(buf));
	url << "https://www.shoutirc.com/youtubesearch.php?q=";
	char * tmp = api->curl->escape(query, strlen(query));
	url << tmp << "&conf=" << buf;
	api->curl->free(tmp);

	char * file = get_cached_download(url.str().c_str(), fn, errbuf, sizeof(errbuf));
	if (file) {
		memset(&y, 0, sizeof(y));
		if (!stricmp(Get_INI_String_Memory(file, "result", "error", errbuf, sizeof(errbuf), "Unknown Error"), "ok")) {
			sstrcpy(y.video_id, Get_INI_String_Memory(file, "result", "id", buf, sizeof(buf), ""));
			sstrcpy(y.title, Get_INI_String_Memory(file, "result", "title", buf, sizeof(buf), "Unknown Title"));
			sstrcpy(y.poster, Get_INI_String_Memory(file, "result", "poster", buf, sizeof(buf), "Unknown Poster"));
			snprintf(y.video_url, sizeof(y.video_url), "https://www.youtube.com/watch?v=%s", y.video_id);
			snprintf(buf, sizeof(buf), "Found %s from channel %s", y.title, y.poster);
			ut->std_reply(ut, buf);
			zfree(file);
			return true;
		} else {
			snprintf(buf, sizeof(buf), "Error searching for video: %s!", errbuf);
			ut->std_reply(ut, buf);
		}
		zfree(file);
	} else {
		snprintf(buf, sizeof(buf), "Error searching for video: %s!", errbuf);
		ut->std_reply(ut, buf);
	}
	return false;
}


int AutoDJ_YT_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "youtube-dl") || !stricmp(command, "youtube-play")) {
		if (parms == NULL || *parms == 0) {
			ut->std_reply(ut, "Usage: !youtube-dl/!youtube-play URL");
			return 1;
		}

		bool do_delete = false;
		bool do_play = (stricmp(command, "youtube-play") == 0) ? true : false;

		const char * url = parms;
		if (do_play && !strnicmp(url, "once ", 5)) {
			do_delete = true;
			url += 5;
		}
		YOUTUBESEARCH ys;
		if (strnicmp(url, "http://", 7) && strnicmp(url, "https://", 7)) {
			if (get_video_id(ut, url, ys)) {
				url = ys.video_url;
				//			ut->std_reply(ut, "Invalid URL! Should start with https:// or http://");
			} else {
				return 1;
			}
		}

		if (ytDownloading.Get() >= 2) {
			ut->std_reply(ut, "I am already downloading 2+ videos, please try again later.");
			return 1;
		}
		ytDownloading.Increment();

		/*
		static time_t last_update = 0;
		if (time(NULL) - last_update >= 7200) {
			system("youtube-dl --update");
			last_update = time(NULL);
		}
		*/

		char path[MAX_PATH],fn[MAX_PATH],fnpat[MAX_PATH];
		if (ad_config.Options.YouTubeDir[0]) {
			sstrcpy(path, ad_config.Options.YouTubeDir);
		} else {
			sstrcpy(path, "./youtube");
		}
		if (access(path, 0) != 0) {
			dsl_mkdir(path, 0755);
		}
		sstrcat(path, PATH_SEPS);

		char video_id[72];

		if (!hashdata("sha256", (const uint8 *)url, strlen(url), video_id, sizeof(video_id))) {
			api->ib_printf(_("AutoDJ -> Error hashing URL! Using fallback...\n"));
			uint8 tmp[16];
			fill_random_buffer(tmp, sizeof(tmp));
			sstrcpy(video_id, "RND");
			bin2hex(tmp, sizeof(tmp), &video_id[3], sizeof(video_id) - 4);
		}

		char * q = api->db->MPrintf("SELECT `FN` FROM `AutoDJ_YouTube` WHERE `URL`=%Q", video_id);
		memset(fn, 0, sizeof(fn));
		api->db->Query(q, get_url_cb, fn, NULL);
		api->db->Free(q);

		string ffn = path;
		ffn += fn;

		if (fn[0] == 0 || access(ffn.c_str(), 0) != 0) {
			char buf[1024];

			snprintf(fn, sizeof(fn), "%s.mp3", video_id);
			snprintf(fnpat, sizeof(fnpat), "%s%s%s", path, video_id, ".%(ext)s");

			ffn = path;
			ffn += fn;

			stringstream sstr;
#if defined(WIN32)
			sstr << "youtube-dl.exe";
#else
			sstr << "youtube-dl";
#endif
			sstr << " --playlist-items 1 --no-playlist -x --audio-format mp3 --add-metadata --metadata-from-title \"%(title)s\" " << ad_config.Options.YouTubeExtra << " -o " << escapeshellarg(fnpat, buf, sizeof(buf));
			sstr << " " << escapeshellarg(url, buf, sizeof(buf));
//#if defined(DEBUG)
			api->ib_printf("AutoDJ -> youtube-dl command line: %s\n", sstr.str().c_str());
//#endif
			ut->std_reply(ut, "Downloading video...");
			system(sstr.str().c_str());
			struct stat64 st;
			if (stat64(ffn.c_str(), &st) == 0 && st.st_size > 128 * 1024) {
				TITLE_DATA td;
				int tdr = ReadMetaData(ffn.c_str(), &td);
				if (!do_delete && tdr == 1 && td.title[0]) {
					snprintf(fn, sizeof(fn), "%s.mp3", td.title);
					char *p = fn;
					while (*p) {
						if (*p == 32 || *p == 33 || *p == 40 || *p == 41 || (*p >= 43 && *p <= 46) || (*p >= 48 && *p <= 57) || (*p >= 65 && *p <= 91) || *p == 93 || *p == 95 || (*p >= 97 && *p <= 122)) {
							p++;
						} else {
							memmove(p, p + 1, strlen(p));
						}
					}
					strtrim(fn);
					if (stricmp(fn, ".mp3")) {
						string ffn2 = path;
						ffn2 += fn;
						if (rename(ffn.c_str(), ffn2.c_str()) == 0) {
							api->ib_printf(_("AutoDJ -> Renamed %s to %s...\n"), ffn.c_str(), ffn2.c_str());
							q = api->db->MPrintf("REPLACE INTO `AutoDJ_YouTube` (`URL`,`FN`) VALUES (%Q, %Q)", video_id, fn);
							api->db->Query(q, NULL, NULL, NULL);
							api->db->Free(q);

							ffn = ffn2;
						} else {
							api->ib_printf(_("AutoDJ -> Error renaming %s to %s: %s\n"), ffn.c_str(), ffn2.c_str(), strerror(errno));
						}
					}
				}
				if (do_play) {
					snprintf(buf, sizeof(buf), "Video '%s' downloaded successfully, queued for playback...", url);
					ut->std_reply(ut, buf);
					QUEUE * Scan = AllocQueue();
					Scan->fn = zstrdup(ffn.c_str());
					Scan->path = zstrdup("");
					if (do_delete) {
						Scan->flags |= QUEUE_FLAG_DELETE_AFTER;
					}
					if (tdr == 1 && td.title[0]) {
						Scan->meta = znew(TITLE_DATA);
						memcpy(Scan->meta, &td, sizeof(TITLE_DATA));
					}
					LockMutex(QueueMutex);
					AddRequest(Scan, ut->User ? ut->User->Nick : ut->Nick);
					RelMutex(QueueMutex);
				} else {
					snprintf(buf, sizeof(buf), "Video '%s' downloaded successfully.", video_id);
					ut->std_reply(ut, buf);
				}
			} else {
				snprintf(buf, sizeof(buf), "Error downloading video '%s'!", url);
				ut->std_reply(ut, buf);
#if !defined(DEBUG)
				remove(ffn.c_str());
#endif
			}
		} else {
			if (do_play) {
				ut->std_reply(ut, "A file with that video ID already exists, queueing now...");
				QUEUE * Scan = AllocQueue();
				Scan->fn = zstrdup(fn);
				Scan->path = zstrdup(path);
				LockMutex(QueueMutex);
				AddRequest(Scan, ut->User ? ut->User->Nick : ut->Nick);
				RelMutex(QueueMutex);
			} else {
				ut->std_reply(ut, "A file with that video ID already exists!");
			}
		}

		ytDownloading.Decrement();
		return 1;
	}
	return 0;
}
