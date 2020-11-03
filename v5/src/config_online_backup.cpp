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
#include "../../Common/libzip/zip.h"
//#include <curl/curl.h>

//#pragma comment(lib, "libcurl-nossl.lib")

char * file_get_contents(const char * fn, long * fLen) {
	FILE * fp = fopen(fn, "rb");
	char * ret = NULL;
	if (fLen) { *fLen = 0; }
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		if (fLen) { *fLen = len; }
		fseek(fp, 0, SEEK_SET);
		ret = (char *)zmalloc(len);
		if (fread(ret, len, 1, fp) != 1) {
			zfree(ret);
			ret = NULL;
		}
		fclose(fp);
	}
	return ret;
}

const char * backup_files[] = {
	IRCBOT_CONF,
	IRCBOT_TEXT,
	"schedule.conf",
	IRCBOT_DB,
	"client.pem",
	"ircbot.pem",
	"autodj.pem",
	NULL
};

bool calculate_config_hash(char * digest, size_t digestLen) { /// digest should be 41+ bytes long
	HASH_CTX * ctx = hash_init("sha1");
	if (ctx == NULL) {
		return NULL;
	}
	for (int i=0; backup_files[i] != NULL; i++) {
		long len = 0;
		char * p = file_get_contents(backup_files[i], &len);
		if (p) {
			//sha1.addBytes(p, len);
			hash_update(ctx, (uint8 *)p, len);
			zfree(p);
		}
	}
	uint8 tmp[20];
	hash_finish(ctx, tmp, sizeof(tmp));
	bin2hex(tmp, 20, digest, digestLen);
	//sha1.getHexDigest(digest, digestLen);
	return true;
}

bool make_zip_file(const char * fn) {
	remove(fn);
	zip * z = zip_open(fn, ZIP_CREATE, NULL);
	if (z == NULL) {
		remove(fn);
		z = zip_open(fn, ZIP_CREATE, NULL);
	}
	if (z == NULL) {
		return NULL;
	}

	for (int i=0; backup_files[i] != NULL; i++) {
		if (access(backup_files[i], 0) == 0) {
			zip_source * src = zip_source_file(z, backup_files[i], 0, 0);
			if (src) {
				if (zip_add(z, backup_files[i], src) == -1) {
					ib_printf(_("OnlineBackup -> Error adding %s to ZIP file!\n"), backup_files[i]);
					zip_source_free(src);
					zip_close(z);
					return false;
				}
			} else {
				ib_printf(_("OnlineBackup -> Error reading %s for ZIP file!\n"), backup_files[i]);
				zip_close(z);
				return false;
			}
		}
	}

	return zip_close(z) == 0 ? true:false;
}

size_t memWrite(void *ptr, size_t size, size_t nmemb, void *stream) {
	MEMWRITE * s = (MEMWRITE *)stream;
	size_t len =  size*nmemb;
	s->data = (char *)zrealloc(s->data, s->len+len+1);
	memcpy(s->data+s->len, ptr, len);
	s->len += len;
	s->data[s->len] = 0;
	return len;
};

void upload_config_backup(const char * hash) {
	ib_printf(_("OnlineBackup -> Creating zip file...\n"));
	if (!make_zip_file("config_backup.zip")) {
		ib_printf(_("OnlineBackup -> Error creating ZIP file!\n"), hash);
		return;
	}

	ib_printf(_("OnlineBackup -> Connecting to server...\n"), hash);
	CURL * h = curl_easy_init();
	if (h != NULL) {
		MEMWRITE stream;
		char errbuf[CURL_ERROR_SIZE];
		memset(errbuf, 0, sizeof(errbuf));
		memset(&stream, 0, sizeof(stream));
		curl_easy_setopt(h, CURLOPT_URL, "http://www.shoutirc.com/online_backup.php");
		curl_easy_setopt(h, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, memWrite);
		curl_easy_setopt(h, CURLOPT_WRITEDATA, &stream);
		curl_easy_setopt(h, CURLOPT_FAILONERROR, 1);
		curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);

		struct curl_httppost *post=NULL, *last=NULL;
		curl_formadd(&post, &last, CURLFORM_COPYNAME, "mode", CURLFORM_COPYCONTENTS, "upload", CURLFORM_END);
		curl_formadd(&post, &last, CURLFORM_COPYNAME, "license", CURLFORM_COPYCONTENTS, config.base.reg_key.c_str(), CURLFORM_END);
		curl_formadd(&post, &last, CURLFORM_COPYNAME, "hash", CURLFORM_COPYCONTENTS, hash, CURLFORM_END);
		curl_formadd(&post, &last, CURLFORM_COPYNAME, "backup", CURLFORM_FILE, "config_backup.zip", CURLFORM_END);

		curl_easy_setopt(h, CURLOPT_HTTPPOST, post);
		ib_printf(_("OnlineBackup -> Uploading file to server...\n"), hash);
		CURLcode res = curl_easy_perform(h);
		if (res == CURLE_OK) {
			if (stream.data) {
				if (strstr(stream.data, "UPLOADED")) {
					ib_printf(_("OnlineBackup -> Configuration backed up successfully!\n"));
				} else if (strstr(stream.data, "ALREADY_HAVE")) {
					ib_printf(_("OnlineBackup -> Server already has current config...\n"));
				} else {
					ib_printf(_("OnlineBackup -> Error from server: %s!\n"), stream.data);
				}
			} else {
				ib_printf(_("OnlineBackup -> Error uploading file: empty reply from server!\n"));
			}
		} else {
			ib_printf(_("OnlineBackup -> Error uploading file: %s!\n"), errbuf);
		}
		curl_easy_cleanup(h);
		curl_formfree(post);
		if (stream.data) { zfree(stream.data); }
	} else {
		ib_printf(_("OnlineBackup -> Error creating curl handle!\n"));
	}
}

void config_online_backup() {
	char hash[41], url[1024];
	if (!calculate_config_hash(hash, sizeof(hash))) {
		ib_printf(_("OnlineBackup -> Error calculating hash for online backup!\n"));
		return;
	}
	ib_printf(_("OnlineBackup -> Configuration hash: %s\n"), hash);

	stringstream surl;
	surl << "http://www.shoutirc.com/online_backup.php?mode=check&license=" << config.base.reg_key << "&hash=" << hash;
	sstrcpy(url, surl.str().c_str());
	CURL * h = curl_easy_init();
	if (h != NULL) {
		MEMWRITE stream;
		char errbuf[CURL_ERROR_SIZE];
		memset(errbuf, 0, sizeof(errbuf));
		memset(&stream, 0, sizeof(stream));
		curl_easy_setopt(h, CURLOPT_URL, url);
		curl_easy_setopt(h, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, memWrite);
		curl_easy_setopt(h, CURLOPT_WRITEDATA, &stream);
		curl_easy_setopt(h, CURLOPT_FAILONERROR, 1);
		curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);
		CURLcode res = curl_easy_perform(h);
		bool doUpload = false;
		if (res == CURLE_OK) {
			if (stream.data) {
				if (strstr(stream.data, "UPLOAD")) {
					doUpload = true;
				} else if (strstr(stream.data, "ALREADY_HAVE")) {
					ib_printf(_("OnlineBackup -> Server already has current config...\n"));
				} else {
					ib_printf(_("OnlineBackup -> Error from server: %s!\n"), stream.data);
				}
			} else {
				ib_printf(_("OnlineBackup -> Error uploading file: empty reply from server!\n"));
			}
		} else {
			ib_printf(_("OnlineBackup -> Error checking if upload is needed: %s!\n"), errbuf);
		}
		curl_easy_cleanup(h);
		if (stream.data) { zfree(stream.data); }
		if (doUpload) {
			upload_config_backup(hash);
		}
	} else {
		ib_printf(_("OnlineBackup -> Error creating curl handle!\n"));
	}
}

