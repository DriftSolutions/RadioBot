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
#include "build.h"

/*
int get_rand(int min, int max) {
	if (min > max) {
		int tmp = min;
		min = max;
		max = tmp;
	}
	return (genrand_int32()%(max-min+1))+min;
}
*/

bool get_range(const char * buf, int32 * imin, int32 * imax) {
	char * p = zstrdup(buf);
	char * q = strchr(p, ':');
	if (q) {
		*q = 0;
		q++;
		*imin = atoi(p);
		*imax = atoi(q);
		if (*imax < *imin) {
			ib_printf("%s: Ranged variable error: %d is smaller than %d!\n", IRCBOT_NAME, *imax, *imin);
			*imax = *imin;
		}
	} else {
		*imin = *imax = atoi(p);
	}
	zfree(p);
	return true;
}

bool is_numeric(char ch) {
	if(ch >= '0' && ch <= '9')
		return true;
	return false;
}

bool is_request_number(const char * p) {
	const char * op = p;
	while (*p != 0) {
		if (*p >= '0' && *p <= '9') {
			p++;
			continue;
		}
		if (op != p && *p == ':') { break; }
		return false;
	}
	return true;
}

string number_format(uint64 n, char sep) {
    stringstream fmt;
    fmt << n;
    string s = fmt.str();
    s.reserve(s.length() + s.length() / 3);

    // loop until the end of the string and use j to keep track of every
    // third loop starting taking into account the leading x digits (this probably
    // can be rewritten in terms of just i, but it seems more clear when you use
    // a seperate variable)
    for (size_t i = 0, j = 3 - s.length() % 3; i < s.length(); ++i, ++j)
        if (i != 0 && j % 3 == 0)
            s.insert(i++, 1, sep);

    return s;
}


/**
 * Returns a text representation of time passed (2d 1h 15m), call zfree() on the returned pointer
 * @param num an amount of time in seconds
 */
char * duration(uint32 num) {
	char * buf = (char *)zmalloc(256);
	return duration2(num, buf);
}

/**
 * Returns a text representation of time passed (2d 1h 15m), put into your own supplied buffer
 * @param num an amount of time in seconds
 * @param buf user-supplied buffer to store the string in
 */
char * duration2(uint32 num, char * buf) {
	char buf2[256];
	int w=0,d=0,h=0,m=0;

	//char * buf = (char *)zmalloc(256);

	while (num >= (60 * 60 * 24 * 7)) {
		num -= (60 * 60 * 24 * 7);
		w++;
	}
	while (num >= (60 * 60 * 24)) {
		num -= (60 * 60 * 24);
		d++;
	}
	while (num >= (60 * 60)) {
		num -= (60 * 60);
		h++;
	}
	while (num >= 60) {
		num -= 60;
		m++;
	}

	buf[0]=0;
	if (w) {
		sprintf(buf,"%dw ",w);
	}
	if (d) {
		sprintf(buf2,"%dd ",d);
		strcat(buf,buf2);
	}
	if (h) {
		sprintf(buf2,"%dh ",h);
		strcat(buf,buf2);
	}
	if (m) {
		sprintf(buf2,"%dm ",m);
		strcat(buf,buf2);
	}
	if (num || buf[0] == 0) {
		sprintf(buf2,"%ds ",num);
		strcat(buf,buf2);
	}

	strtrim(buf," ");
	return buf;
}

int32 decode_duration(const char * buf) {
	int ret=0;
	int num=0;
	for (size_t i=0; i < strlen(buf); i++) {
		if (is_numeric(buf[i])) {
			num *= 10;
			num += (buf[i]-'0');
		} else if (buf[i] == 'y') {
			ret += (num * 31536000);
			num = 0;
		} else if (buf[i] == 'd') {
			ret += (num * 86400);
			num = 0;
		} else if (buf[i] == 'h') {
			ret += (num * 3600);
			num = 0;
		} else if (buf[i] == 'm') {
			ret += (num * 60);
			num = 0;
		} else if (buf[i] == 's') {
			ret += num;
			num = 0;
		}
	}
	return ret+num;
}

void utprintf(USER_PRESENCE * up, USER_PRESENCE::userPresSendFunc sendfunc, const char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
#if defined(WIN32)
	int len = vscprintf(fmt, va);
	char * ret = (char *)zmalloc(len+1);
	vsprintf(ret, fmt, va);
	sendfunc(up, ret);
	zfree(ret);
#else
	char * tmp = NULL;// *ret = NULL;
	if (vasprintf(&tmp, fmt, va) != -1 && tmp) {
		sendfunc(up, tmp);
		free(tmp);
	} else {
		sendfunc(up, "vasprintf error!!!");
	}
#endif
	va_end(va);
}

/**
 * Returns true if the specified message exists, false otherwise
 * @param name a string specifying the message name
 */
bool IsMessage(const char * name) {
	message_list::iterator x = config.messages.find(name);
	if (x == config.messages.end()) {
		return false;
	} else {
		return true;
	}
}

/**
 * Loads a message into a buffer. Returns false if the message doesn't exist, true if it does.
 * @param name a string specifying the message name
 * @param msgbuf a user-supplied buffer to store the message in
 * @param bufSize the size in bytes of msgbuf (will be passed to strlcpy/snprintf directly
 */
bool LoadMessage(const char * name, char * msgbuf, int32 bufSize) {
	message_list::iterator x = config.messages.find(name);
	if (x != config.messages.end()) {
		memset(msgbuf, 0, bufSize);
		strlcpy(msgbuf,x->second.c_str(), bufSize);
		return true;
	}
	snprintf(msgbuf,bufSize,_("%s: Couldn't load message '%s'\r\n"), IRCBOT_NAME, name);
	return false;
}

/**
 * Loads a message for a specific channel into a buffer, loads the global message if no channel-specific one exists. Returns false if the message doesn't exist, true if it does.
 * @param name a string specifying the message name
 * @param netno the network number the channel is on
 * @param chan the channel the message is being loaded for
 * @param msgbuf a user-supplied buffer to store the message in
 * @param bufSize the size in bytes of msgbuf (will be passed to strlcpy/snprintf directly
 */
bool LoadMessageChannel(const char * msg, int netno, const char * chan, char * msgbuf, int32 bufSize) {
	char * tmp = zmprintf("%s_%d_%s", msg, netno, chan);
	bool ret = LoadMessage(tmp, msgbuf, bufSize);
	zfree(tmp);
	if (ret) { return ret; }
	return LoadMessage(msg, msgbuf, bufSize);
}

#if defined(IRCBOT_ENABLE_SS)
/**
 * Return's the current DJ display text (equivalent to %dj)
 * You must zfree() the result of this function!
 */
char * GetCurrentDJDisp() {
	string dj = config.stats.curdj;
	LockMutex(requestMutex);
	if (HAVE_DJ(config.base.req_mode) && config.base.dj_name_mode != DJNAME_MODE_STANDARD) {
		if (config.req_user) {
			if (config.base.dj_name_mode == DJNAME_MODE_USERNAME && config.req_user->User) {
				dj = config.req_user->User->Nick;
			} else {
				dj = config.req_user->Nick;
			}
		} else {
#if defined(IRCBOT_ENABLE_IRC)
			dj = config.base.irc_nick;
#else
			dj = "AutoDJ";
#endif
		}
	}
	RelMutex(requestMutex);
	return zstrdup(dj.c_str());
}
#endif


/**
 * Does text replacement for variables
 * @param text a string possibly containing variables to be processed
 * @param bufsize the size of text in bytes
 */
void ProcText(char * text, int32 bufsize) {
	char buf[8192];

	LockMutex(sesMutex);
	for (message_list::iterator x = config.customVars.begin(); x != config.customVars.end(); x++) {
		str_replaceA(text,bufsize,x->first.c_str(),x->second.c_str());
	}
	RelMutex(sesMutex);

	IBM_PROCTEXT ibpt;
	ibpt.buf = text;
	ibpt.bufSize = bufsize;
	SendMessage(-1, IB_PROCTEXT, (char *)&ibpt, sizeof(ibpt));

	str_replaceA(text,bufsize,"%color","\x03");
	str_replaceA(text,bufsize,"%bold","\x02");
	str_replaceA(text,bufsize,"%action%","\x01");
	str_replaceA(text,bufsize,"%under%","\x1F");
	str_replaceA(text,bufsize,"%italic%","\x1D");
	str_replaceA(text,bufsize,"\\n","\n");
	str_replaceA(text,bufsize,"\\r","\r");

#if defined(IRCBOT_ENABLE_SS)
	//LockMutex(requestMutex);
	if (requestMutex.Lock(1000)) {
		if (HAVE_DJ(config.base.req_mode)) {
			if (config.base.req_mode == REQ_MODE_HOOKED) {
	#if defined(IRCBOT_ENABLE_IRC)
				str_replaceA(text,bufsize, "%djnick%", config.base.irc_nick.c_str());
				str_replaceA(text,bufsize, "%djuser%", config.base.irc_nick.c_str());
	#else
				str_replaceA(text,bufsize, "%djnick%", "AutoDJ");
				str_replaceA(text,bufsize, "%djuser%", "AutoDJ");
	#endif
			} else {
				str_replaceA(text,bufsize, "%djnick%", config.req_user ? config.req_user->Nick:"");
				str_replaceA(text,bufsize, "%djuser%", (config.req_user && config.req_user->User) ? config.req_user->User->Nick:"");
			}
		} else {
			str_replaceA(text,bufsize, "%djnick%", "");
			str_replaceA(text,bufsize, "%djuser%", "");
		}
		RelMutex(requestMutex);
	}
	char * dj = GetCurrentDJDisp();
	str_replaceA(text,bufsize,"%dj",dj);
	zfree(dj);
	str_replaceA(text,bufsize,"%olddj",config.stats.lastdj);
	//str_replace(text,bufsize,"%playlist",config.base.playlist.c_str());
	str_replaceA(text,bufsize,"%song",config.stats.cursong);

	str_replaceA(text, bufsize, "%ratings%", (config.base.EnableRatings) ? "On":"Off");
	if (config.base.EnableRatings && config.stats.online && config.stats.cursong[0]) {
		if (stristr(text, "%rating%") || stristr(text, "%votes%")) {
			SONG_RATING r;
			uint32 id = 0;
			if (SendMessage(-1, SOURCE_GET_SONGID, (char *)&id, sizeof(id)) == 1 && id != 0) {
				GetSongRating(id, &r);
			} else {
				GetSongRating(config.stats.cursong, &r);
			}
			sprintf(buf, "%u", r.Rating);
			str_replaceA(text, bufsize, "%rating%", buf);
			sprintf(buf, "%u", r.Votes);
			str_replaceA(text, bufsize, "%votes%", buf);
		}
	} else {
		str_replaceA(text, bufsize, "%rating%", "0");
		str_replaceA(text, bufsize, "%votes%", "0");
	}

	if (config.base.multiserver > 0) {
		char find[32];
		for (int i=0; i < config.num_ss; i++) {
			sprintf(find, "%%ss(%d).status%%", i);
			str_replace(text, bufsize, find, config.s_stats[i].online ? "Online":"Offline");
			sprintf(find, "%%ss(%d).dj%%", i);
			str_replace(text, bufsize, find, config.s_stats[i].curdj);
			sprintf(find, "%%ss(%d).song%%", i);
			str_replace(text, bufsize, find, config.s_stats[i].cursong);

			sprintf(find, "%%ss(%d).clients%%", i);
			sprintf(buf,"%d",config.s_stats[i].listeners);
			str_replace(text, bufsize, find, buf);
			sprintf(find, "%%ss(%d).peak%%", i);
			sprintf(buf,"%d",config.s_stats[i].peak);
			str_replace(text, bufsize, find, buf);
			sprintf(find, "%%ss(%d).max%%", i);
			sprintf(buf,"%d",config.s_stats[i].maxusers);
			str_replace(text, bufsize, find, buf);
		}
	}

	sprintf(buf, "%s %s", IRCBOT_NAME, GetBotVersionString());
#else
	sprintf(buf, "%s Lite %s", IRCBOT_NAME, GetBotVersionString());
#endif
	str_replaceA(text,bufsize,"%version", buf);
	str_replaceA(text,bufsize,"%build_date", __DATE__);
	str_replaceA(text,bufsize,"%build_time", __TIME__);
	str_replaceA(text,bufsize,"%platform", PLATFORM);
	str_replaceA(text,bufsize,"%build","[Build #" BUILD_STRING "]");
	str_replaceA(text,bufsize,"%pathsep%", PATH_SEPS);
	str_replaceA(text,bufsize,"%soext%", DL_SOEXT);

	if (strstr(text,"%uptime")) {
		char * d = duration(int32(time(NULL) - config.start_time));
		str_replaceA(text,bufsize,"%uptime",d);
		zfree(d);
	}

#if defined(IRCBOT_ENABLE_SS)
	if (config.base.enable_requests && REQUESTS_ON(config.base.req_mode)) {
		strcpy(buf,_("On"));
	} else {
		strcpy(buf,_("Off"));
	}
	str_replaceA(text,bufsize,"%req",buf);
	sprintf(buf,"%d",config.stats.listeners);
	str_replaceA(text,bufsize,"%clients",buf);
	sprintf(buf,"%d",config.stats.peak);
	str_replaceA(text,bufsize,"%peak",buf);
	sprintf(buf,"%d",config.stats.maxusers);
	str_replaceA(text,bufsize,"%max",buf);
#endif
}

int mask_num_types() {
	return 4;
}

#if defined(DEBUG)
void mask_test() {
	char * masks[3] = {
		"nick!khaled@mirc.com",
		"ShoutIRC!~ircbot@B78F6CAB.B6D32247.FFEEB95E.IP",
		"Indy!~indy@netadmin.shoutirc.com"
	};
	char buf[256];
	for (int j=0; j < 3; j++) {
		ib_printf("Mask test: %s\n", masks[j]);
		for (int i=0; i < mask_num_types(); i++) {
			mask(masks[j], buf, sizeof(buf), i);
			ib_printf("Type %d: %s\n", i, buf);
		}
	}
}
#endif

/*
 0: *!*user@*.host
 1: *!*@*.host
 2: *nick*!*user@*.host
 3: *nick*!*@*.host
*/
bool mask(const char * hostmask, char * buf, size_t bufSize, int type) {
	char * tmp = zstrdup(hostmask);
	char * user = strchr(tmp, '!');
	if (user == NULL) {
		zfree(tmp);
		return false;
	}
	*user = 0;
	user++;
	char * host = strchr(user, '@');
	if (host == NULL) {
		zfree(tmp);
		return false;
	}
	*host = 0;
	host++;

	bool ret = true;
	if (type == 0) {
		if (*user == '~') { user++; }
		StrTokenizer st(host, '.');
		stringstream sstr;
		sstr << "*!*" << user << "@";
		//char * hostp = NULL;
		if (st.NumTok() > 2) {
			sstr << "*.";
			sstr << st.stdGetTok(st.NumTok()-1,st.NumTok());
		} else {
			sstr << st.stdGetTok(1,st.NumTok());
		}
		strlcpy(buf, sstr.str().c_str(), bufSize);
	} else if (type == 1) {
		StrTokenizer st(host, '.');
		stringstream sstr;
		sstr << "*!*@";
		//char * hostp = NULL;
		if (st.NumTok() > 2) {
			sstr << "*.";
			sstr << st.stdGetTok(st.NumTok()-1,st.NumTok());
		} else {
			sstr << st.stdGetTok(1,st.NumTok());
		}
		strlcpy(buf, sstr.str().c_str(), bufSize);
	} else if (type == 2) {
		if (*user == '~') { user++; }
		StrTokenizer st(host, '.');
		stringstream sstr;
		sstr << "*" << tmp << "*!*" << user << "@";
		//char * hostp = NULL;
		if (st.NumTok() > 2) {
			sstr << "*.";
			sstr << st.stdGetTok(st.NumTok()-1,st.NumTok());
		} else {
			sstr << st.stdGetTok(1,st.NumTok());
		}
		strlcpy(buf, sstr.str().c_str(), bufSize);
	} else if (type == 3) {
		StrTokenizer st(host, '.');
		stringstream sstr;
		sstr << "*" << tmp << "*!*@";
		//char * hostp = NULL;
		if (st.NumTok() > 2) {
			sstr << "*.";
			sstr << st.stdGetTok(st.NumTok()-1,st.NumTok());
		} else {
			sstr << st.stdGetTok(1,st.NumTok());
		}
		strlcpy(buf, sstr.str().c_str(), bufSize);
	} else {
		ret = false;
	}

	zfree(tmp);
	return ret;
}
