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

#include "tts_priv.h"

size_t ttsWrite(void *ptr, size_t size, size_t nmemb, void *stream) {
	FILE * fp = (FILE *)stream;
	size_t len =  size*nmemb;
	if (fwrite(ptr, len, 1, fp) == 1) {
		return len;
	}
	return 0;
};

bool MakeMP3FromFile_GTrans(TTS_JOB * job) {
	stringstream url;
	url << "http://translate.google.com/translate_tts?";//tl=en&q=text
	if (job->VoiceOverride[0]) {
		url << "tl=" << job->VoiceOverride << "&";
	}
	url << "q=";

	FILE * fp = fopen(job->inputFN, "rb");
	if (fp == NULL) {
		return false;
	}
	char msgbuf[8192];
	memset(msgbuf, 0, sizeof(msgbuf));
	fread(msgbuf, 1, sizeof(msgbuf)-1, fp);
	fclose(fp);
	fp = fopen(job->outputFN, "wb");
	if (fp == NULL) {
		return false;
	}

	char * q = api->curl->escape(msgbuf, strlen(msgbuf));
	url << q;
	api->curl->free(q);

	bool ret = false;
	CURL * h = api->curl->easy_init();
	if (h != NULL) {
		char errbuf[CURL_ERROR_SIZE];
		memset(errbuf, 0, sizeof(errbuf));
		char * surl = zstrdup(url.str().c_str());
		api->curl->easy_setopt(h, CURLOPT_URL, surl);
		api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
		api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1);
		api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, ttsWrite);
		api->curl->easy_setopt(h, CURLOPT_WRITEDATA, fp);
		api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10);
		api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 30);
		api->curl->easy_setopt(h, CURLOPT_FAILONERROR, 1);
		api->curl->easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);

		api->ib_printf(_("TTS -> Google Translate -> Requesting TTS MP3...\n"), errbuf);
		CURLcode res = api->curl->easy_perform(h);
		if (res == CURLE_OK) {
			long len = ftell(fp);
			api->ib_printf(_("TTS -> Google Translate -> Downloaded TTS MP3 successfully! (%d bytes)\n"), len);
			ret = true;
		} else {
			api->ib_printf(_("TTS -> Google Translate -> Error downloading TTS MP3: %s!\n"), errbuf);
		}
		api->curl->easy_cleanup(h);
		zfree(surl);
	} else {
		api->ib_printf(_("TTS -> Google Translate -> Error creating curl handle!\n"));
	}

	fclose(fp);
	return ret;
}

bool MakeWAVFromFile_GTrans(TTS_JOB * job) {
	return Generic_MakeWAVFromFile(job);
}

TTS_ENGINE tts_engine_gtrans = {
	VE_GTRANS,
	MakeMP3FromFile_GTrans,
	MakeWAVFromFile_GTrans
};

