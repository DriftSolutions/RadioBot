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

Titus_Mutex requestMutex;

/**
 * Grabs the pointer to the Request System mutex. As with all RadioBot mutexes it is recursive.
 */
//Titus_Mutex * getRequestMutex() { return &requestMutex; }

/**
 * Get's the nick of the person/bot currently DJing. Will return the first one it can of the following list:
 * 1. User's internal RadioBot username
 * 2. User's current IRC/medium nickname
 * 3. If req_mode is REQ_MODE_HOOKED, the bot's base IRC nick
 * 4. Unknown
 * The return from this function should be zfree'ed.
 */
char * GetRequestDJNick() {
	AutoMutex(requestMutex);
	if (config.base.enable_requests && HAVE_DJ(config.base.req_mode)) {
		if (config.req_user) {
			return zstrdup(config.req_user->User ? config.req_user->User->Nick:config.req_user->Nick);
		} else if (config.base.req_mode == REQ_MODE_HOOKED) {
#if defined(IRCBOT_ENABLE_IRC)
			return zstrdup(config.base.irc_nick.c_str());
#else
			return zstrdup("AutoDJ");
#endif
		} else {
			return zstrdup(_("Unknown"));
		}
	}
	return zstrdup(_("No DJ"));
}

void RequestsOff() {
	AutoMutex(requestMutex);
	if (config.base.req_mode != REQ_MODE_OFF) {
		IBM_REQONOFF ir;
		memset(&ir, 0, sizeof(ir));
		ir.userpres = config.req_user;
		SendMessage(-1, IB_REQ_OFF, (char *)&ir, sizeof(IBM_REQONOFF));
	}
	config.base.req_mode = REQ_MODE_OFF;
	if (config.req_user) {
		USER_PRESENCE * user = config.req_user;
#if defined(IRCBOT_ENABLE_IRC)
		if (config.base.req_modes_on_login.length() && !stricmp(user->Desc, "IRC") && user->NetNo >= 0 && user->NetNo < config.num_irc && config.ircnets[user->NetNo].ready) {
			char buf[1024];
			for (int i=0; i < config.ircnets[user->NetNo].num_chans; i++) {
				if (config.ircnets[user->NetNo].ial->IsNickOnChan(config.ircnets[user->NetNo].channels[i].channel.c_str(), user->Nick)) {
					snprintf(buf, sizeof(buf), "MODE %s -%s", config.ircnets[user->NetNo].channels[i].channel.c_str(), config.base.req_modes_on_login.c_str());
					for (size_t j=0; j < config.base.req_modes_on_login.length(); j++) {
						strlcat(buf, " ", sizeof(buf));
						strlcat(buf, user->Nick, sizeof(buf));
					}
					strlcat(buf, "\r\n", sizeof(buf));
					bSend(config.ircnets[user->NetNo].sock, buf);
				}
			}
		}
#endif
		UnrefUser(config.req_user);
	}
	config.req_user = NULL;
	config.req_iface = NULL;
}

void RequestsOn(USER_PRESENCE * user, REQUEST_MODES mode, REQUEST_INTERFACE * iface) {
	AutoMutex(requestMutex);
	RequestsOff();
	if (!config.base.enable_requests) {
		return;
	}
	config.base.req_mode = mode;
	config.req_user = user;
	config.req_iface = iface;
	if (user) {
		RefUser(user);
#if defined(IRCBOT_ENABLE_IRC)
		if (config.base.req_modes_on_login.length() && !stricmp(user->Desc, "IRC") && user->NetNo >= 0 && user->NetNo < config.num_irc && config.ircnets[user->NetNo].ready) {
			char buf[1024];
			for (int i=0; i < config.ircnets[user->NetNo].num_chans; i++) {
				if (config.ircnets[user->NetNo].ial->IsNickOnChan(config.ircnets[user->NetNo].channels[i].channel.c_str(), user->Nick)) {
					snprintf(buf, sizeof(buf), "MODE %s +%s", config.ircnets[user->NetNo].channels[i].channel.c_str(), config.base.req_modes_on_login.c_str());
					for (size_t j=0; j < config.base.req_modes_on_login.length(); j++) {
						strlcat(buf, " ", sizeof(buf));
						strlcat(buf, user->Nick, sizeof(buf));
					}
					strlcat(buf, "\r\n", sizeof(buf));
					bSend(config.ircnets[user->NetNo].sock, buf);
				}
			}
		}
#endif
	}
	IBM_REQONOFF ir;
	memset(&ir, 0, sizeof(ir));
	ir.userpres = user;
	SendMessage(-1, IB_REQ_ON, (char *)&ir, sizeof(IBM_REQONOFF));
}

void RequestsOn(REQUEST_INTERFACE * iface) {
	AutoMutex(requestMutex);
	RequestsOff();
	if (config.base.enable_requests) {
		config.base.req_mode = REQ_MODE_HOOKED;
		config.req_user = NULL;
		config.req_iface = iface;
	}
}

/**
 * Delivers a request to the current DJ.
 * @param from a USER_PRESENCE handle
 * @param req a string containing the request text
 */
bool DeliverRequest(USER_PRESENCE * from, const char * req, char * reply, size_t replySize) {
	bool ret = false;
	AutoMutex(requestMutex);
	char buf[1024];
	if (REQUESTS_ON(config.base.req_mode) && config.req_user) {
		if (config.base.req_mode == REQ_MODE_REMOTE) {
			ib_printf(_("Sending request to remote user...\n"));
			REMOTE_HEADER rHead;
			if (from->Channel) {
				sprintf(buf,_("%s\xFE^- %s in %s on %s"), req, from->Nick, from->Channel, from->Desc);
			} else {
				sprintf(buf,_("%s\xFE^- %s on %s"), req, from->Nick, from->Desc);
			}
			rHead.scmd=RCMD_REQ_INCOMING;
			rHead.datalen=strlen(buf);
			SendRemoteReply(config.req_user->Sock, &rHead, buf);
			LoadMessage("ReqThanks", reply, replySize);
			ret = true;
		} else if (config.base.req_mode == REQ_MODE_NORMAL) {
			ib_printf(_("Sending to user on %s...\n"), config.req_user->Desc);
			if (from->Channel) {
				if (!LoadMessage("RequestFromChan", buf, sizeof(buf))) {
					strlcpy(buf,"Request from %nick in %chan on %proto: %req", sizeof(buf));
				}
				str_replace(buf, sizeof(buf), "%chan", from->Channel);
			} else {
				if (!LoadMessage("RequestFromNick", buf, sizeof(buf))) {
					strlcpy(buf,"Request from %nick on %proto: %req", sizeof(buf));
				}
			}
			str_replace(buf, sizeof(buf), "%nick", from->Nick);
			str_replace(buf, sizeof(buf), "%proto", from->Desc);
			str_replace(buf, sizeof(buf), "%req", req);
			ProcText(buf, sizeof(buf));
			config.req_user->send_msg(config.req_user, buf);
			LoadMessage("ReqThanks", reply, replySize);
			ret = true;
		}
	} else if (config.base.req_mode == REQ_MODE_HOOKED && config.req_iface) {
		ret = config.req_iface->request(from, req, reply, replySize);
	} else if (config.base.req_mode == REQ_MODE_DJ_NOREQ && config.req_user && LoadMessage("ReqDJNoRequests", reply, replySize)) {
		//ret = true;
#if defined(IRCBOT_ENABLE_IRC)
	} else if (config.base.req_fallback.length() && config.ircnets[0].ready) {
		if (from->Channel) {
			if (!LoadMessage("RequestFromChan", buf, sizeof(buf))) {
				strlcpy(buf,"Request from %nick in %chan on %proto: %req", sizeof(buf));
			}
			str_replace(buf, sizeof(buf), "%chan", from->Channel);
		} else {
			if (!LoadMessage("RequestFromNick", buf, sizeof(buf))) {
				strlcpy(buf,"Request from %nick on %proto: %req", sizeof(buf));
			}
		}
		str_replace(buf, sizeof(buf), "%nick", from->Nick);
		str_replace(buf, sizeof(buf), "%proto", from->Desc);
		str_replace(buf, sizeof(buf), "%req", req);
		ProcText(buf, sizeof(buf));
		char * buf2 = zmprintf("PRIVMSG %s :%s\r\n", config.base.req_fallback.c_str(), buf);
		bSend(config.ircnets[0].sock, buf2, 0, PRIORITY_DEFAULT);
		zfree(buf2);
		LoadMessage("ReqFallback", reply, replySize);
		ret = true;
#endif
	} else {
		LoadMessage("ReqNoDJ", reply, replySize);
	}
	return ret;
}

bool DeliverRequest(USER_PRESENCE * from, FIND_RESULT * result, char * reply, size_t replySize) {
	bool ret = false;
	AutoMutex(requestMutex);
//	char buf[1024];
	if (config.base.req_mode == REQ_MODE_HOOKED && config.req_iface) {
		ret = config.req_iface->request_from_find(from, result, reply, replySize);
	} else {
		ret = DeliverRequest(from, result->fn, reply, replySize);
	}
	return ret;
}

class FindResultTrack {
public:
	FindResultTrack() { timestamp = time(NULL); res = NULL; }
	FindResultTrack(FIND_RESULTS * pres) { timestamp = time(NULL); res = pres; }
	time_t timestamp;
	FIND_RESULTS * res;
};
typedef std::map<string, FindResultTrack, ci_less> findResultList;
findResultList findResults;
string GenFindID(USER_PRESENCE * ut) {
	stringstream ret;
	if (ut->User) {
		ret << "user_" << ut->Nick;
	} else {
		ret << ut->Hostmask;//ut->Desc << "_" << ut->NetNo << "_" <<
	}
	return ret.str();
}
void SaveFindResult(const char * id, FIND_RESULTS * res) {
	AutoMutex(requestMutex);
	FindResultTrack x(res);
	findResults[id] = x;
}
FIND_RESULTS * GetFindResult(const char * id) {
	AutoMutex(requestMutex);
	findResultList::iterator x = findResults.find(id);
	if (x != findResults.end()) {
		x->second.timestamp = time(NULL);
		return x->second.res;
	}
	return NULL;
}
void ExpireFindResults(bool all, int plugin) {
	AutoMutex(requestMutex);
	time_t exp = time(NULL)-(60*config.base.expire_find_results);
	for (findResultList::iterator x = findResults.begin(); x != findResults.end();) {
		if (all || plugin == x->second.res->plugin || x->second.timestamp < exp) {
			ib_printf("" IRCBOT_NAME ": Expiring find result '%s' ...\n", x->first.c_str());
			x->second.res->free(x->second.res);
			findResults.erase(x);
			x = findResults.begin();
		} else {
			x++;
		}
	}
}

#endif
