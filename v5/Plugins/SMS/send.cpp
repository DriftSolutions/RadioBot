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

#include "sms.h"

#if (LIBCURL_VERSION_MAJOR < 7) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 17) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR == 17 && LIBCURL_VERSION_PATCH < 1)
#define OLD_CURL2
#else
#undef OLD_CURL2
#endif

size_t gv_memWrite(void *ptr, size_t size, size_t nmemb, void *stream) {
	MEMWRITE * s = (MEMWRITE *)stream;
	size_t len =  size*nmemb;
	s->data = (char *)zrealloc(s->data, s->len+len+1);
	memcpy(s->data+s->len, ptr, len);
	s->len += len;
	s->data[s->len] = 0;
	return len;
};

GoogleVoice::GoogleVoice(string puser, string ppass) {
	user = puser;
	pass = ppass;
	logged_in = false;
	ch = api->curl->easy_init();
#if defined(WIN32)
	api->curl->easy_setopt(ch, CURLOPT_COOKIEJAR, "./tmp/google.voice.cookie");
#else
	char buf[MAX_PATH];
	sprintf(buf, "/tmp/ircbot.google.voice.cookie.%u", getpid());
	//printf("Voice cookie file: %s\n", buf);
	api->curl->easy_setopt(ch, CURLOPT_COOKIEJAR, buf);
#endif
	api->curl->easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);
	api->curl->easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0);
	api->curl->easy_setopt(ch, CURLOPT_MAXREDIRS, 10);
	api->curl->easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
	api->curl->easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	api->curl->easy_setopt(ch, CURLOPT_VERBOSE, 0);
	api->curl->easy_setopt(ch, CURLOPT_USERAGENT, "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0)");
	api->curl->easy_setopt(ch, CURLOPT_WRITEFUNCTION, gv_memWrite);
	api->curl->easy_setopt(ch, CURLOPT_WRITEDATA, &mem);
}

GoogleVoice::~GoogleVoice() {
	api->curl->easy_cleanup(ch);
	ch = NULL;
}

bool GoogleVoice::logIn() {
	if (logged_in) { return true; }

	// fetch the login page
	memset(&mem, 0, sizeof(mem));
	api->curl->easy_setopt(ch, CURLOPT_URL, "https://accounts.google.com/ServiceLogin?passive=true&service=grandcentral");
	CURLcode ret = api->curl->easy_perform(ch);
	if (ret != CURLE_OK) {
		api->ib_printf("SMS -> Error logging in to Google Voice: %s!\n", api->curl->easy_strerror(ret));
		if (mem.data) { zfree(mem.data); }
		return false;
	}

	char galx[1024];
	memset(galx, 0, sizeof(galx));
	char * p = stristr(mem.data, "name=\"GALX\"");
	if (p) {
		char * q = stristr(p, "value=\"");
		if (q) {
			q += 7;
			char * e = strchr(q, '\"');
			if (e) {
				strncpy(galx, q, e-q);
			} else {
				strlcpy(galx, q, sizeof(galx));
			}
		} else {
			api->ib_printf("SMS -> Could not parse for GALX value!\n");
			if (mem.data) { zfree(mem.data); }
			return false;
		}
	} else {
		api->ib_printf("SMS -> Could not parse for GALX token!\n");
		if (mem.data) { zfree(mem.data); }
		return false;
	}

	if (mem.data) { zfree(mem.data); }
	memset(&mem, 0, sizeof(mem));

	api->curl->easy_setopt(ch, CURLOPT_URL, "https://accounts.google.com/ServiceLoginAuth?service=grandcentral");
	api->curl->easy_setopt(ch, CURLOPT_POST, 1);
	stringstream post;

	char * tmp = api->curl->escape(user.c_str(), user.length());
	post << "Email=" << tmp << "&";
	api->curl->free(tmp);
	tmp = api->curl->escape(pass.c_str(), pass.length());
	post << "Passwd=" << tmp << "&";
	api->curl->free(tmp);
	tmp = api->curl->escape("https://www.google.com/voice/account/signin", 0);
	post << "continue=" << tmp << "&";
	api->curl->free(tmp);
	post << "service=grandcentral&";
	tmp = api->curl->escape(galx, 0);
	post << "GALX=" << tmp;
	api->curl->free(tmp);
#if !defined(OLD_CURL2)
	api->curl->easy_setopt(ch, CURLOPT_COPYPOSTFIELDS, post.str().c_str());
#else
	tmp = zstrdup(post.str().c_str());
	api->curl->easy_setopt(ch, CURLOPT_POSTFIELDS, tmp);
#endif
	ret = api->curl->easy_perform(ch);
#if defined(OLD_CURL2)
	zfree(tmp);
#endif
	api->curl->easy_setopt(ch, CURLOPT_HTTPGET, 1);
	if (ret != CURLE_OK) {
		api->ib_printf("SMS -> Error logging in to Google Voice: %s!\n", api->curl->easy_strerror(ret));
		if (mem.data) { zfree(mem.data); }
		return false;
	}

	memset(galx, 0, sizeof(galx));
	p = stristr(mem.data, "name=\"_rnr_se\"");
	if (p) {
		char * q = stristr(p, "value=\"");
		if (q) {
			q += 7;
			char * e = strchr(q, '\"');
			if (e) {
				strncpy(galx, q, e-q);
			} else {
				strlcpy(galx, q, sizeof(galx));
			}
			google_priv = galx;
		} else {
			api->ib_printf("SMS -> Could not parse for _rnr_se value!\n");
			if (mem.data) { zfree(mem.data); }
			return false;
		}
	} else {
		api->ib_printf("SMS -> Could not parse for _rnr_se token!\n");
		if (mem.data) { zfree(mem.data); }
		return false;
	}
	if (mem.data) { zfree(mem.data); }

	return true;
}

bool GoogleVoice::SendSMS(string phone, string msg) {
	logIn();

	memset(&mem, 0, sizeof(mem));

	api->curl->easy_setopt(ch, CURLOPT_URL, "https://www.google.com/voice/sms/send/");
	api->curl->easy_setopt(ch, CURLOPT_POST, 1);
	stringstream post;

	char * tmp = api->curl->escape(google_priv.c_str(), 0);
	post << "_rnr_se=" << tmp << "&";
	api->curl->free(tmp);
	tmp = api->curl->escape(phone.c_str(), 0);
	post << "phoneNumber=" << tmp << "&";
	api->curl->free(tmp);
	tmp = api->curl->escape(msg.c_str(), 0);
	post << "text=" << tmp << "";
	api->curl->free(tmp);
#if !defined(OLD_CURL2)
	api->curl->easy_setopt(ch, CURLOPT_COPYPOSTFIELDS, post.str().c_str());
#else
	tmp = zstrdup(post.str().c_str());
	api->curl->easy_setopt(ch, CURLOPT_POSTFIELDS, tmp);
#endif
	CURLcode ret = api->curl->easy_perform(ch);
#if defined(OLD_CURL2)
	zfree(tmp);
#endif
	api->curl->easy_setopt(ch, CURLOPT_HTTPGET, 1);
	if (ret != CURLE_OK) {
		api->ib_printf("SMS -> Error sending SMS message via Google Voice: %s!\n", api->curl->easy_strerror(ret));
		if (mem.data) { zfree(mem.data); }
		return false;
	}

	bool bret = (stristr(mem.data, "\"ok\":true") != NULL) ? true:false;
	if (!bret) { logged_in = false; }

#if defined(DEBUG)
	FILE * fp = fopen("googresp.log", "wb");
	if (fp) {
		fwrite(mem.data, mem.len, 1, fp);
		fclose(fp);
	}
#endif
	if (mem.data) { zfree(mem.data); }
	return bret;
}

msgMap sendMessages;
Titus_Mutex sendMutex;

TT_DEFINE_THREAD(Send_Thread) {
	TT_THREAD_START
	GoogleVoice * gv = new GoogleVoice(sms_config.voice.user, sms_config.voice.pass);
	//gv->SendSMS("8282098665", "Test");
	while (!sms_config.shutdown_now) {
		sendMutex.Lock();
		int tries=0;
		msgMap::iterator x = sendMessages.begin();
		while (x != sendMessages.end() && !sms_config.shutdown_now && tries < 5) {
			StrTokenizer st((char *)x->second.c_str(), '\n');
			string to = x->first;
			string msg;
			bool erase = false;
			if (st.NumTok() > 1 && x->second.length() > 140) {
				long i;
				msg = st.stdGetSingleTok(1);
				for (i=2; i <= st.NumTok() && sms_config.LineMerge; i++) {
					string cur = st.stdGetSingleTok(i);
					if ((msg.length() + 1 + cur.length()) <= 140) {
						msg += "\n";
						msg += cur;
					} else { break; }
				}
				if (msg.length() >= x->second.length()) {
					erase = true;
				}
			} else {
				msg = x->second;
				erase = true;
			}
			sendMutex.Release();

			api->ib_printf("SMS -> Sending to phone %s: %s\n", to.c_str(), msg.c_str());
			/*
			string msg2 = msg;
			size_t n;
			while ((n = msg2.find("\n")) != string::npos) {
				msg2.replace(n, 1, "%0A");
			}
			*/
			if (gv->SendSMS(to, msg)) {
				api->ib_printf("SMS -> Message sent!\n");
				tries=0;
			} else {
				api->ib_printf("SMS -> Error sending message!\n");
				erase = false;
				tries++;
				safe_sleep(1);
			}

			sendMutex.Lock();
			if (erase) {
				sendMessages.erase(x);
			} else if (tries == 0) {
				x->second.erase(0, msg.length());
				while (x->second.c_str()[0] == '\n') { x->second.erase(0, 1); }
			}

			x = sendMessages.begin();
		}
		sendMutex.Release();
		safe_sleep(1);
	}
	delete gv;
	sendMutex.Lock();
	sendMessages.clear();
	sendMutex.Release();
	TT_THREAD_END
}

void Send_SMS(string phone, string text) {
	sendMutex.Lock();
	if (sms_config.LineMerge) {
		const char * p = text.c_str() + text.length() - 1;
		if (*p != '.' && *p == '!' && *p == '?') {
			text += ".";
		}
	}
	msgMap::iterator x = sendMessages.find(phone);
	if (x != sendMessages.end()) {
		x->second += "\n";
		x->second += text;
	} else {
		sendMessages[phone] = text;
	}
	/*
	SMS_Message msg;
	msg.to = phone;
	msg.message = text;
	sendMessages.push_back(msg);
	*/
	sendMutex.Release();
}
